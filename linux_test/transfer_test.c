#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>

#include "../linux_driver/systolic_array_ioctl.h"

#define WORDS_PER_BUF 16

static int copy_to_dma(int fd, sa_u32 channel, void *ptr, sa_u32 num_bytes)
{
    struct sa_dma_transfer xfer;

    memset(&xfer, 0, sizeof(xfer));
    xfer.channel = channel;
    xfer.offset = 0;
    xfer.num_bytes = num_bytes;
    xfer.user_ptr = (sa_u64)(uintptr_t)ptr;

    if (ioctl(fd, SA_IOC_COPY_TO_DMA, &xfer) < 0) {
        perror("SA_IOC_COPY_TO_DMA");
        return -1;
    }

    return 0;
}

static int copy_from_dma(int fd, sa_u32 channel, void *ptr, sa_u32 num_bytes)
{
    struct sa_dma_transfer xfer;

    memset(&xfer, 0, sizeof(xfer));
    xfer.channel = channel;
    xfer.offset = 0;
    xfer.num_bytes = num_bytes;
    xfer.user_ptr = (sa_u64)(uintptr_t)ptr;

    if (ioctl(fd, SA_IOC_COPY_FROM_DMA, &xfer) < 0) {
        perror("SA_IOC_COPY_FROM_DMA");
        return -1;
    }

    return 0;
}

static int run_accelerator(int fd, sa_u32 num_bytes)
{
    struct sa_run_config cfg;

    memset(&cfg, 0, sizeof(cfg));

    cfg.mm2s0_bytes = num_bytes;
    cfg.mm2s1_bytes = num_bytes;
    cfg.mm2s2_bytes = num_bytes;
    cfg.s2mm_bytes = num_bytes;
    cfg.timeout_us = 1000000;

    if (ioctl(fd, SA_IOC_RUN, &cfg) < 0) {
        perror("SA_IOC_RUN");
        return -1;
    }

    return 0;
}

int main(void)
{
    int fd;
    uint32_t mm2s0[WORDS_PER_BUF];
    uint32_t mm2s1[WORDS_PER_BUF];
    uint32_t mm2s2[WORDS_PER_BUF];
    uint32_t out[WORDS_PER_BUF];
    sa_u32 num_bytes = WORDS_PER_BUF * sizeof(uint32_t);

    fd = open("/dev/systolic_array", O_RDWR);
    if (fd < 0) {
        perror("open /dev/systolic_array");
        return 1;
    }

    for (int i = 0; i < WORDS_PER_BUF; i++) {
        mm2s0[i] = 0x10000000u + i;
        mm2s1[i] = 0x20000000u + i;
        mm2s2[i] = 0x30000000u + i;
        out[i] = 0;
    }

    if (copy_to_dma(fd, SA_DMA_CH_MM2S0, mm2s0, num_bytes))
        goto fail;

    if (copy_to_dma(fd, SA_DMA_CH_MM2S1, mm2s1, num_bytes))
        goto fail;

    if (copy_to_dma(fd, SA_DMA_CH_MM2S2, mm2s2, num_bytes))
        goto fail;

    if (run_accelerator(fd, num_bytes))
        goto fail;

    if (copy_from_dma(fd, SA_DMA_CH_S2MM, out, num_bytes))
        goto fail;

    printf("Output words:\n");
    for (int i = 0; i < WORDS_PER_BUF; i++)
        printf("out[%02d] = 0x%08x\n", i, out[i]);

    close(fd);
    return 0;

fail:
    close(fd);
    return 1;
}