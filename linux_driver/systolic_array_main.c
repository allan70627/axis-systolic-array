/*
 * Systolic Array Linux Driver
 *
 * This is the register-level Linux driver entry point for the AXI systolic array.
 *
 * Current scope:
 *   - Map AXI-Lite registers from the device tree
 *   - Create /dev/systolic_array
 *   - Allow simple register read/write for debugging
 *   - Support ioctl-based register read/write
 *   - Support start/wait helper functions through systolic_array_hw.c
 *
 * Future work:
 *   - Add DMA/coherent buffer allocation
 *   - Add full input/output matrix transfer support
 *   - Add Python/PYNQ wrapper
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/errno.h>

#include "systolic_array_priv.h"
#include "systolic_array_regs.h"
#include "systolic_array_ioctl.h"
#include "systolic_array_hw.h"

#define DRIVER_NAME "systolic_array"

/*
 * Open operation.
 *
 * For miscdevice, file->private_data initially points to struct miscdevice.
 * Convert it once here so read/write/ioctl can all use struct sa_dev directly.
 */
static int sa_open(struct inode *inode, struct file *file)
{
    struct miscdevice *miscdev = file->private_data;
    struct sa_dev *sa = container_of(miscdev, struct sa_dev, miscdev);

    file->private_data = sa;
    return 0;
}

/*
 * /dev/systolic_array read operation.
 *
 * This is a simple debug/status interface. Running:
 *
 *   cat /dev/systolic_array
 *
 * prints several important control/status registers.
 */
static ssize_t sa_read_file(struct file *file, char __user *buf,
                            size_t count, loff_t *ppos)
{
    struct sa_dev *sa = file->private_data;
    char tmp[256];
    int len;

    len = scnprintf(tmp, sizeof(tmp),
                    "START=0x%08x\n"
                    "MM2S_0_DONE=0x%08x\n"
                    "MM2S_1_DONE=0x%08x\n"
                    "MM2S_2_DONE=0x%08x\n"
                    "S2MM_DONE=0x%08x\n"
                    "DONE_STATUS=0x%08x\n",
                    sa_hw_read_reg(sa, A_START),
                    sa_hw_read_reg(sa, A_MM2S_0_DONE),
                    sa_hw_read_reg(sa, A_MM2S_1_DONE),
                    sa_hw_read_reg(sa, A_MM2S_2_DONE),
                    sa_hw_read_reg(sa, A_S2MM_DONE),
                    sa_hw_done_status(sa));

    return simple_read_from_buffer(buf, count, ppos, tmp, len);
}

/*
 * /dev/systolic_array write operation.
 *
 * Expected user input format:
 *
 *   <register_index> <hex_value>
 *
 * Example:
 *
 *   echo "4 12345678" | sudo tee /dev/systolic_array
 *
 * This writes 0x12345678 to register index 4.
 *
 * This string-based interface is kept only as a simple debug path.
 * The cleaner interface is ioctl.
 */
static ssize_t sa_write_file(struct file *file, const char __user *buf,
                             size_t count, loff_t *ppos)
{
    struct sa_dev *sa = file->private_data;
    char tmp[64];
    unsigned int reg;
    unsigned int value;
    int ret;

    if (count >= sizeof(tmp))
        return -EINVAL;

    if (copy_from_user(tmp, buf, count))
        return -EFAULT;

    tmp[count] = '\0';

    ret = sscanf(tmp, "%u %x", &reg, &value);
    if (ret != 2)
        return -EINVAL;

    /*
     * Prevent accidental accesses outside the known register map.
     */
    if (reg > SA_MAX_REG_INDEX)
        return -EINVAL;

    sa_hw_write_reg(sa, reg, value);

    return count;
}

/*
 * ioctl interface.
 *
 * This provides structured register access from user space.
 */
static long sa_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct sa_dev *sa = file->private_data;
    struct sa_reg_access reg;
    sa_u32 status;
    int ret;

    switch (cmd) {
    case SA_IOC_READ_REG:
        if (copy_from_user(&reg, (void __user *)arg, sizeof(reg)))
            return -EFAULT;

        if (reg.reg > SA_MAX_REG_INDEX)
            return -EINVAL;

        reg.value = sa_hw_read_reg(sa, reg.reg);

        if (copy_to_user((void __user *)arg, &reg, sizeof(reg)))
            return -EFAULT;

        return 0;

    case SA_IOC_WRITE_REG:
        if (copy_from_user(&reg, (void __user *)arg, sizeof(reg)))
            return -EFAULT;

        if (reg.reg > SA_MAX_REG_INDEX)
            return -EINVAL;

        sa_hw_write_reg(sa, reg.reg, reg.value);
        return 0;

    case SA_IOC_START:
        sa_hw_start(sa);
        return 0;

    case SA_IOC_WAIT:
        /*
         * Blocking wait skeleton.
         *
         * Wait up to 1 second for all visible done bits to assert.
         * Later, this can be replaced with interrupt-based waiting.
         */
        ret = sa_hw_wait_done(sa, 1000000);
        if (ret)
            return ret;

        status = sa_hw_done_status(sa);

        if (copy_to_user((void __user *)arg, &status, sizeof(status)))
            return -EFAULT;

        return 0;

    default:
        return -ENOTTY;
    }
}

static const struct file_operations sa_fops = {
    .owner = THIS_MODULE,
    .open = sa_open,
    .read = sa_read_file,
    .write = sa_write_file,
    .unlocked_ioctl = sa_ioctl,
};

/*
 * Probe is called when Linux finds a device tree node with:
 *
 *   compatible = "custom,systolic-array";
 *
 * The probe function maps the AXI-Lite register region and registers
 * the misc device, which creates /dev/systolic_array.
 */
static int sa_probe(struct platform_device *pdev)
{
    struct sa_dev *sa;
    struct resource *res;
    int ret;

    sa = devm_kzalloc(&pdev->dev, sizeof(*sa), GFP_KERNEL);
    if (!sa)
        return -ENOMEM;

    sa->dev = &pdev->dev;

    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    if (!res)
        return -ENODEV;

    sa->regs = devm_ioremap_resource(&pdev->dev, res);
    if (IS_ERR(sa->regs))
        return PTR_ERR(sa->regs);

    sa->miscdev.minor = MISC_DYNAMIC_MINOR;
    sa->miscdev.name = "systolic_array";
    sa->miscdev.fops = &sa_fops;
    sa->miscdev.parent = &pdev->dev;

    ret = misc_register(&sa->miscdev);
    if (ret)
        return ret;

    platform_set_drvdata(pdev, sa);

    dev_info(&pdev->dev,
             "systolic array driver probed, mapped register region %pR\n",
             res);

    return 0;
}

/*
 * Remove is called when the driver is unloaded or the device is removed.
 */
static int sa_remove(struct platform_device *pdev)
{
    struct sa_dev *sa = platform_get_drvdata(pdev);

    misc_deregister(&sa->miscdev);

    return 0;
}

/*
 * Device tree match table.
 *
 * This compatible string must match the device tree node.
 */
static const struct of_device_id sa_of_match[] = {
    { .compatible = "custom,systolic-array" },
    { }
};
MODULE_DEVICE_TABLE(of, sa_of_match);

/*
 * Platform driver registration.
 */
static struct platform_driver sa_platform_driver = {
    .probe = sa_probe,
    .remove = sa_remove,
    .driver = {
        .name = DRIVER_NAME,
        .of_match_table = sa_of_match,
    },
};

module_platform_driver(sa_platform_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Register-level Linux driver for AXI systolic array");