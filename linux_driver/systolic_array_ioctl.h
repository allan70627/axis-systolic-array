#ifndef SYSTOLIC_ARRAY_IOCTL_H
#define SYSTOLIC_ARRAY_IOCTL_H

#ifdef __KERNEL__
#include <linux/ioctl.h>
#include <linux/types.h>
typedef __u32 sa_u32;
#else
#include <sys/ioctl.h>
#include <stdint.h>
typedef uint32_t sa_u32;
#endif

#define SA_IOC_MAGIC 's'

struct sa_reg_access {
    sa_u32 reg;
    sa_u32 value;
};

struct sa_dma_info {
    sa_u32 in_dma_addr;
    sa_u32 out_dma_addr;
    sa_u32 buf_size;
};

#define SA_IOC_READ_REG     _IOWR(SA_IOC_MAGIC, 1, struct sa_reg_access)
#define SA_IOC_WRITE_REG    _IOW(SA_IOC_MAGIC, 2, struct sa_reg_access)
#define SA_IOC_START        _IO(SA_IOC_MAGIC, 3)
#define SA_IOC_WAIT         _IOR(SA_IOC_MAGIC, 4, sa_u32)
#define SA_IOC_GET_DMA_INFO _IOR(SA_IOC_MAGIC, 5, struct sa_dma_info)

#endif