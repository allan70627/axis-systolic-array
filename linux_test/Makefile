CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra
INCLUDES := -I../linux_driver

all: reg_test ioctl_test dma_info_test transfer_test

reg_test: reg_test.c ../linux_driver/systolic_array_regs.h
	$(CC) $(CFLAGS) $(INCLUDES) reg_test.c -o reg_test

ioctl_test: ioctl_test.c ../linux_driver/systolic_array_ioctl.h
	$(CC) $(CFLAGS) $(INCLUDES) ioctl_test.c -o ioctl_test

dma_info_test: dma_info_test.c ../linux_driver/systolic_array_ioctl.h
	$(CC) $(CFLAGS) $(INCLUDES) dma_info_test.c -o dma_info_test

transfer_test: transfer_test.c ../linux_driver/systolic_array_ioctl.h
	$(CC) $(CFLAGS) $(INCLUDES) transfer_test.c -o transfer_test

clean:
	rm -f reg_test ioctl_test dma_info_test transfer_test