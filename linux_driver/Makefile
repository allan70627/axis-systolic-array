obj-m += systolic_array_drv.o

systolic_array_drv-objs := \
	systolic_array_main.o \
	systolic_array_hw.o \
	systolic_array_dma.o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean