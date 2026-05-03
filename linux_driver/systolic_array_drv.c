/*
 * Systolic Array Linux Driver
 *
 * This is the first register-level Linux driver for the AXI systolic array.
 * It maps the AXI-Lite control register space and exposes a simple
 * /dev/systolic_array debug interface.
 *
 * Current scope:
 *   - Map AXI-Lite registers from the device tree
 *   - Create /dev/systolic_array
 *   - Allow simple register read/write for debugging
 *   - Add ioctl-based register access
 *
 * Future work:
 *   - Add DMA/coherent buffer allocation
 *   - Add full input/output matrix transfer support
 *   - Add correctness comparison against golden output
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/slab.h>

#include "systolic_array_ioctl.h"
#include "systolic_array_regs.h"

#define DRIVER_NAME "systolic_array"

/*
 * Per-device driver state.
 *
 * regs:
 *   Kernel virtual address of the AXI-Lite register region.
 *
 * miscdev:
 *   Misc character device used to create /dev/systolic_array.
 */
struct sa_dev {
    void __iomem *regs;
    struct miscdevice miscdev;
};

/*
 * Convert register index to byte address.
 *
 * The hardware register map uses register indices, not byte offsets.
 * For example, register index 0x4 corresponds to byte offset 0x10
 * because each AXI-Lite register is 4 bytes.
 */
static inline void __iomem *sa_reg_addr(struct sa_dev *sa, u32 reg_index)
{
    return sa->regs + reg_index * REG_STRIDE_BYTES;
}

/*
 * Read one 32-bit AXI-Lite register.
 */
static inline u32 sa_read(struct sa_dev *sa, u32 reg_index)
{
    return ioread32(sa_reg_addr(sa, reg_index));
}

/*
 * Write one 32-bit AXI-Lite register.
 */
static inline void sa_write(struct sa_dev *sa, u32 reg_index, u32 value)
{
    iowrite32(value, sa_reg_addr(sa, reg_index));
}

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
 * This is a debug/status interface. Running:
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
                    "S2MM_DONE=0x%08x\n",
                    sa_read(sa, A_START),
                    sa_read(sa, A_MM2S_0_DONE),
                    sa_read(sa, A_MM2S_1_DONE),
                    sa_read(sa, A_MM2S_2_DONE),
                    sa_read(sa, A_S2MM_DONE));

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
     * Prevent accidental writes outside the known register map.
     */
    if (reg > SA_MAX_REG_INDEX)
        return -EINVAL;

    sa_write(sa, reg, value);

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
    __u32 status;

    switch (cmd) {
    case SA_IOC_READ_REG:
        if (copy_from_user(&reg, (void __user *)arg, sizeof(reg)))
            return -EFAULT;

        if (reg.reg > SA_MAX_REG_INDEX)
            return -EINVAL;

        reg.value = sa_read(sa, reg.reg);

        if (copy_to_user((void __user *)arg, &reg, sizeof(reg)))
            return -EFAULT;

        return 0;

    case SA_IOC_WRITE_REG:
        if (copy_from_user(&reg, (void __user *)arg, sizeof(reg)))
            return -EFAULT;

        if (reg.reg > SA_MAX_REG_INDEX)
            return -EINVAL;

        sa_write(sa, reg.reg, reg.value);
        return 0;

    case SA_IOC_START:
        sa_write(sa, A_START, 1);
        return 0;

    case SA_IOC_WAIT:
        /*
         * First skeleton version:
         * Return 1 only if all visible DMA done bits are 1.
         * Later this can become a real blocking wait with timeout.
         */
        status = sa_read(sa, A_MM2S_0_DONE) &
                 sa_read(sa, A_MM2S_1_DONE) &
                 sa_read(sa, A_MM2S_2_DONE) &
                 sa_read(sa, A_S2MM_DONE);

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