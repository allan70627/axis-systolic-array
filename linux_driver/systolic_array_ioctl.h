#ifndef SYSTOLIC_ARRAY_IOCTL_H
#define SYSTOLIC_ARRAY_IOCTL_H

#ifdef __KERNEL__
#include <linux/ioctl.h>
#include <linux/types.h>
typedef __u32 sa_u32;
typedef __u64 sa_u64;
#else
#include <sys/ioctl.h>
#include <stdint.h>
typedef uint32_t sa_u32;
typedef uint64_t sa_u64;
#endif

#define SA_IOC_MAGIC 's'

#define SA_DMA_CH_MM2S0 0
#define SA_DMA_CH_MM2S1 1
#define SA_DMA_CH_MM2S2 2
#define SA_DMA_CH_S2MM  3

struct sa_reg_access {
    sa_u32 reg;
    sa_u32 value;
};

struct sa_dma_info {
    sa_u32 mm2s0_dma_addr;
    sa_u32 mm2s1_dma_addr;
    sa_u32 mm2s2_dma_addr;
    sa_u32 s2mm_dma_addr;
    sa_u32 buf_size;
};

/*
 * Generic DMA copy request.
 *
 * channel:
 *   SA_DMA_CH_MM2S0, SA_DMA_CH_MM2S1, SA_DMA_CH_MM2S2, or SA_DMA_CH_S2MM
 *
 * offset:
 *   byte offset inside the selected DMA buffer
 *
 * num_bytes:
 *   number of bytes to copy
 *
 * user_ptr:
 *   user-space pointer cast to sa_u64
 */
struct sa_dma_transfer {
    sa_u32 channel;
    sa_u32 offset;
    sa_u32 num_bytes;
    sa_u64 user_ptr;
};

/*
 * Run configuration.
 *
 * Each byte count tells the driver how many bytes from each DMA buffer
 * should be presented to the hardware.
 */
struct sa_run_config {
    sa_u32 mm2s0_bytes;
    sa_u32 mm2s1_bytes;
    sa_u32 mm2s2_bytes;
    sa_u32 s2mm_bytes;
    sa_u32 timeout_us;
};

#define SA_IOC_READ_REG      _IOWR(SA_IOC_MAGIC, 1, struct sa_reg_access)
#define SA_IOC_WRITE_REG     _IOW(SA_IOC_MAGIC, 2, struct sa_reg_access)
#define SA_IOC_START         _IO(SA_IOC_MAGIC, 3)
#define SA_IOC_WAIT          _IOR(SA_IOC_MAGIC, 4, sa_u32)
#define SA_IOC_GET_DMA_INFO  _IOR(SA_IOC_MAGIC, 5, struct sa_dma_info)
#define SA_IOC_COPY_TO_DMA   _IOW(SA_IOC_MAGIC, 6, struct sa_dma_transfer)
#define SA_IOC_COPY_FROM_DMA _IOWR(SA_IOC_MAGIC, 7, struct sa_dma_transfer)
#define SA_IOC_RUN           _IOW(SA_IOC_MAGIC, 8, struct sa_run_config)

#endif