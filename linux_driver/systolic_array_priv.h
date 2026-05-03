#ifndef SYSTOLIC_ARRAY_PRIV_H
#define SYSTOLIC_ARRAY_PRIV_H

#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/types.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>

#define SA_DMA_BUF_SIZE 4096

struct sa_dma_buffer {
    void *virt;
    dma_addr_t dma;
    size_t size;
};

struct sa_dev {
    struct device *dev;
    void __iomem *regs;
    struct miscdevice miscdev;

    struct sa_dma_buffer in_buf;
    struct sa_dma_buffer out_buf;
};

#endif