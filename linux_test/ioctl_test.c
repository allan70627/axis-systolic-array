#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "../linux_driver/systolic_array_ioctl.h"

int main(void)
{
    int fd;
    struct sa_reg_access reg;

    fd = open("/dev/systolic_array", O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    reg.reg = 4;
    reg.value = 0x12345678;

    if (ioctl(fd, SA_IOC_WRITE_REG, &reg) < 0) {
        perror("ioctl write");
        close(fd);
        return 1;
    }

    reg.value = 0;

    if (ioctl(fd, SA_IOC_READ_REG, &reg) < 0) {
        perror("ioctl read");
        close(fd);
        return 1;
    }

    printf("Register %u = 0x%08x\n", reg.reg, reg.value);

    close(fd);
    return 0;
}