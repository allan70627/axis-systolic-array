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
 *   - Support start/wait helper functions
 *   - Allocate coherent DMA input/output buffers
 *
 * Future work:
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
#include "systolic_array_dma.h"

#define DRIVER_NAME "systolic_array"

static int sa_open(struct inode *inode, struct file *file)
{
    struct miscdevice *miscdev = file->private_data;
    struct sa_dev *sa = container_of(miscdev, struct sa_dev, miscdev);

    file->private_data = sa;
    return 0;
}

static ssize_t sa_read_file(struct file *file, char __user *buf,
                            size_t count, loff_t *ppos)
{
    struct sa_dev *sa = file->private_data;
    char tmp[384];
    int len;

    len = scnprintf(tmp, sizeof(tmp),
                    "START=0x%08x\n"
                    "MM2S_0_DONE=0x%08x\n"
                    "MM2S_1_DONE=0x%08x\n"
                    "MM2S_2_DONE=0x%08x\n"
                    "S2MM_DONE=0x%08x\n"
                    "DONE_STATUS=0x%08x\n"
                    "IN_DMA=0x%pad\n"
                    "OUT_DMA=0x%pad\n"
                    "DMA_BUF_SIZE=%zu\n",
                    sa_hw_read_reg(sa, A_START),
                    sa_hw_read_reg(sa, A_MM2S_0_DONE),
                    sa_hw_read_reg(sa, A_MM2S_1_DONE),
                    sa_hw_read_reg(sa, A_MM2S_2_DONE),
                    sa_hw_read_reg(sa, A_S2MM_DONE),
                    sa_hw_done_status(sa),
                    &sa->in_buf.dma,
                    &sa->out_buf.dma,
                    sa->in_buf.size);

    return simple_read_from_buffer(buf, count, ppos, tmp, len);
}

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

    if (reg > SA_MAX_REG_INDEX)
        return -EINVAL;

    sa_hw_write_reg(sa, reg, value);

    return count;
}

static long sa_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    struct sa_dev *sa = file->private_data;
    struct sa_reg_access reg;
    struct sa_dma_info info;
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
        ret = sa_hw_wait_done(sa, 1000000);
        if (ret)
            return ret;

        status = sa_hw_done_status(sa);

        if (copy_to_user((void __user *)arg, &status, sizeof(status)))
            return -EFAULT;

        return 0;

    case SA_IOC_GET_DMA_INFO:
        info.in_dma_addr = (sa_u32)sa->in_buf.dma;
        info.out_dma_addr = (sa_u32)sa->out_buf.dma;
        info.buf_size = (sa_u32)sa->in_buf.size;

        if (copy_to_user((void __user *)arg, &info, sizeof(info)))
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

    ret = sa_dma_init(sa);
    if (ret)
        return ret;

    sa->miscdev.minor = MISC_DYNAMIC_MINOR;
    sa->miscdev.name = "systolic_array";
    sa->miscdev.fops = &sa_fops;
    sa->miscdev.parent = &pdev->dev;

    ret = misc_register(&sa->miscdev);
    if (ret) {
        sa_dma_cleanup(sa);
        return ret;
    }

    platform_set_drvdata(pdev, sa);

    dev_info(&pdev->dev,
             "systolic array driver probed, mapped register region %pR\n",
             res);

    return 0;
}

static int sa_remove(struct platform_device *pdev)
{
    struct sa_dev *sa = platform_get_drvdata(pdev);

    misc_deregister(&sa->miscdev);
    sa_dma_cleanup(sa);

    return 0;
}

static const struct of_device_id sa_of_match[] = {
    { .compatible = "custom,systolic-array" },
    { }
};
MODULE_DEVICE_TABLE(of, sa_of_match);

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