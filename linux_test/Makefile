CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra
INCLUDES := -I../linux_driver

all: reg_test ioctl_test

reg_test: reg_test.c ../linux_driver/systolic_array_regs.h
	$(CC) $(CFLAGS) $(INCLUDES) reg_test.c -o reg_test

ioctl_test: ioctl_test.c ../linux_driver/systolic_array_ioctl.h
	$(CC) $(CFLAGS) $(INCLUDES) ioctl_test.c -o ioctl_test

clean:
	rm -f reg_test ioctl_test