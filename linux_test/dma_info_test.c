#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "../linux_driver/systolic_array_ioctl.h"

int main(void)
{
    int fd;
    struct sa_dma_info info;

    fd = open("/dev/systolic_array", O_RDWR);
    if (fd < 0) {
        perror("open /dev/systolic_array");
        return 1;
    }

    if (ioctl(fd, SA_IOC_GET_DMA_INFO, &info) < 0) {
        perror("SA_IOC_GET_DMA_INFO");
        close(fd);
        return 1;
    }

    printf("MM2S0 DMA address: 0x%08x\n", info.mm2s0_dma_addr);
    printf("MM2S1 DMA address: 0x%08x\n", info.mm2s1_dma_addr);
    printf("MM2S2 DMA address: 0x%08x\n", info.mm2s2_dma_addr);
    printf("S2MM  DMA address: 0x%08x\n", info.s2mm_dma_addr);
    printf("Buffer size      : %u bytes\n", info.buf_size);

    close(fd);
    return 0;
}