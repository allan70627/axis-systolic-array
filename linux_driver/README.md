Systolic Array Linux Driver and Test Workflow

This document explains the Linux driver and user-space test structure for the AXI systolic array accelerator.

The goal of this work is to build a Linux/PYNQ-side software interface for controlling the FPGA accelerator from user space. The systolic array project is being used as a simpler test platform before applying similar driver ideas to the larger CGRA4ML system.

Big Picture

The Linux-side work is split into two folders:

linux_driver/  = kernel-side driver code
linux_test/    = user-space C test programs

The overall software/hardware path is:

User-space test program
        ↓
/dev/systolic_array
        ↓
Linux kernel driver
        ↓
AXI-Lite registers + DMA buffers
        ↓
FPGA systolic array accelerator

The kernel driver owns the hardware-facing resources, such as mapped AXI-Lite registers and coherent DMA buffers. The user-space programs use /dev/systolic_array and ioctl calls to control the accelerator.

Repository Structure
axis-systolic-array/
  linux_driver/
    Makefile
    README.md
    systolic_array_main.c
    systolic_array_priv.h
    systolic_array_hw.c
    systolic_array_hw.h
    systolic_array_dma.c
    systolic_array_dma.h
    systolic_array_ioctl.h
    systolic_array_regs.h
    systolic_array.dts.example

  linux_test/
    Makefile
    README.md
    reg_test.c
    ioctl_test.c
    dma_info_test.c
    transfer_test.c
linux_driver/ Components

The linux_driver/ folder contains the kernel module implementation.

The driver creates:

/dev/systolic_array

and exposes both a simple debug text interface and a structured ioctl interface.

linux_driver/Makefile

The Makefile builds the Linux kernel module.

The final kernel module is:

systolic_array_drv.ko

Internally, the module is built from multiple object files:

systolic_array_main.o
systolic_array_hw.o
systolic_array_dma.o

This file connects the driver source files into one loadable kernel module.

Important note: this Makefile is intended for the target PYNQ Linux system or a proper cross-compilation setup with matching PYNQ kernel headers. It is not expected to build successfully inside normal WSL unless WSL has matching kernel headers.

systolic_array_main.c

This is the main Linux driver entry point.

It handles the Linux-facing part of the driver:

platform driver registration
device tree matching
probe/remove functions
/dev/systolic_array creation
open/read/write/ioctl file operations

Important functions in this file:

sa_probe()       called when Linux finds the FPGA device
sa_remove()      called when the driver is removed
sa_open()        prepares file->private_data correctly
sa_read_file()   debug status read
sa_write_file()  debug register write
sa_ioctl()       handles structured user-space commands

This file is where Linux connects the hardware device to the driver. It binds to the device tree node using the compatible string:

custom,systolic-array

It also exposes the user-space interface:

cat /dev/systolic_array
echo "4 12345678" | sudo tee /dev/systolic_array
ioctl(fd, ...)

The open() function is important because for a miscdevice, file->private_data initially points to a struct miscdevice. The driver converts it into:

struct sa_dev *

so that read, write, and ioctl all access the same internal driver state.

systolic_array_priv.h

This is the private internal driver header.

It defines the driver’s internal data structures, especially:

struct sa_dev
struct sa_dma_buffer

struct sa_dev is the main driver state object. It stores:

device pointer
mapped AXI-Lite register base
misc device object
MM2S DMA buffers
S2MM DMA buffer

This file is only for kernel driver implementation files. User-space tests should not include this file.

In short:

systolic_array_priv.h = internal driver memory/state definition
systolic_array_hw.c

This file contains hardware register helper functions.

It handles low-level AXI-Lite register operations, including:

sa_hw_read_reg()
sa_hw_write_reg()
sa_hw_start()
sa_hw_done_status()
sa_hw_wait_done()

Instead of placing raw register read/write logic inside systolic_array_main.c, the driver keeps that hardware-specific logic here.

This separation makes the structure clearer:

systolic_array_main.c = Linux interface logic
systolic_array_hw.c   = hardware register logic

For example, when user space calls SA_IOC_START, systolic_array_main.c calls:

sa_hw_start(sa);

Then systolic_array_hw.c writes to the A_START register.

systolic_array_hw.h

This is the header for systolic_array_hw.c.

It declares the hardware helper functions so that systolic_array_main.c can call them.

Main declarations include:

sa_hw_read_reg()
sa_hw_write_reg()
sa_hw_start()
sa_hw_done_status()
sa_hw_wait_done()
systolic_array_dma.c

This file contains DMA helper functions.

It manages DMA-safe memory buffers owned by the kernel driver.

It handles:

allocating coherent DMA buffers
freeing coherent DMA buffers
selecting DMA buffers by channel
copying user input into DMA buffers
copying DMA output back to user space

The driver currently supports four DMA channels:

MM2S0
MM2S1
MM2S2
S2MM

Meaning:

MM2S = memory-mapped to stream = input data from memory to hardware
S2MM = stream to memory-mapped = output data from hardware to memory

The conceptual data movement is:

User input array
    ↓ copy_from_user()
Driver MM2S DMA buffer
    ↓ FPGA reads it
Systolic array accelerator
    ↓ FPGA writes result
Driver S2MM DMA buffer
    ↓ copy_to_user()
User output array

This file is important because Linux user-space pointers are virtual addresses, but FPGA DMA needs DMA-safe addresses. The driver uses dma_alloc_coherent() to allocate memory that both the CPU and hardware can safely access.

systolic_array_dma.h

This is the header for systolic_array_dma.c.

It declares DMA helper functions such as:

sa_dma_init()
sa_dma_cleanup()
sa_dma_get_buffer()
sa_dma_copy_to_buffer()
sa_dma_copy_from_buffer()

systolic_array_main.c calls these functions when processing DMA-related ioctl commands.

systolic_array_ioctl.h

This is the shared user/kernel interface header.

It is included by both:

linux_driver/
linux_test/

This file defines the commands that user-space can send to the driver.

Current ioctl commands:

SA_IOC_READ_REG       read one AXI-Lite register
SA_IOC_WRITE_REG      write one AXI-Lite register
SA_IOC_START          start accelerator
SA_IOC_WAIT           wait until done
SA_IOC_GET_DMA_INFO   get allocated DMA buffer addresses
SA_IOC_COPY_TO_DMA    copy user data into driver DMA buffer
SA_IOC_COPY_FROM_DMA  copy output data from driver DMA buffer
SA_IOC_RUN            program DMA registers, start hardware, wait

It also defines shared structs:

struct sa_reg_access      register index/value pair
struct sa_dma_info        DMA address information
struct sa_dma_transfer    copy-to/from-DMA request
struct sa_run_config      byte counts and timeout for running hardware

This file is the contract between user space and kernel space.

systolic_array_regs.h

This is the hardware register map.

It defines register indices such as:

A_START
A_MM2S_0_ADDR
A_MM2S_0_BYTES
A_MM2S_0_DONE
A_MM2S_1_ADDR
A_MM2S_1_BYTES
A_MM2S_1_DONE
A_MM2S_2_ADDR
A_MM2S_2_BYTES
A_MM2S_2_DONE
A_S2MM_ADDR
A_S2MM_BYTES
A_S2MM_DONE

The driver uses these names to know which hardware register to read or write.

Important idea:

register index 4 is not byte address 4
byte offset = register index * REG_STRIDE_BYTES

For example, if each register is 32 bits:

register index 4 → byte offset 16 → 0x10
systolic_array.dts.example

This is the example device tree node.

Linux uses the device tree to know:

what hardware exists
where the register space is located
which driver should bind to the hardware

Example:

systolic_array@40000000 {
    compatible = "custom,systolic-array";
    reg = <0x40000000 0x10000>;
};

The key field is:

compatible = "custom,systolic-array";

This must match the driver’s device tree match table.

The address:

0x40000000

is the expected AXI-Lite register base address for the accelerator.