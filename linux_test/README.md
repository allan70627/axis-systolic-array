# Systolic Array Linux User-Space Tests

This folder contains user-space C test programs for the Linux driver under `../linux_driver`.

These tests are used to validate the Linux/PYNQ software interface for the AXI systolic array accelerator.

Some tests can be compiled in WSL/Docker, but they can only run successfully on the target PYNQ Linux board after the FPGA bitstream is loaded, the kernel driver is inserted, and `/dev/systolic_array` exists.

## File Overview

### `Makefile`

Builds all user-space C test programs.

Current targets:

```text
reg_test
ioctl_test
dma_info_test
transfer_test