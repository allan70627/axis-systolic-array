# Systolic Array Linux Driver

This folder contains the Linux kernel driver work for the AXI systolic array accelerator.

The goal of this driver is to provide a Linux/PYNQ-side software interface for controlling the FPGA accelerator from user space. This work is part of the larger plan to first validate Linux driver concepts on the simpler `axis-systolic-array` project before applying the same structure to CGRA4ML.

## Current Status

The current driver supports:

1. AXI-Lite register access
2. `/dev/systolic_array` character device creation
3. Debug register read/write through simple text-based file operations
4. Structured ioctl-based register access
5. Hardware start/wait helper functions
6. Coherent DMA buffer allocation
7. DMA transfer ioctl support for copying input/output data between user space and driver-owned DMA buffers
8. A run ioctl that programs DMA address/byte registers, starts the accelerator, and waits for completion

The current implementation is ready for code review and user-space build testing in WSL/Docker. Full runtime validation still requires the PYNQ Linux board, the correct device tree node, the loaded FPGA bitstream, and the kernel module built against the target PYNQ kernel.

## Folder Structure

```text
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