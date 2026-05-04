# Systolic Array Linux Driver and Test Workflow

This document explains the Linux driver and user-space test structure for the AXI systolic array accelerator.

The goal of this work is to build a Linux/PYNQ-side software interface for controlling the FPGA accelerator from user space. The systolic array project is being used as a simpler test platform before applying similar driver ideas to the larger CGRA4ML system.

---

## 1. Big Picture

The Linux-side work is split into two folders:

| Folder | Purpose |
|---|---|
| `linux_driver/` | Kernel-side driver code |
| `linux_test/` | User-space C test programs |

The overall software/hardware path is:

```text
User-space test program
        ↓
/dev/systolic_array
        ↓
Linux kernel driver
        ↓
AXI-Lite registers + DMA buffers
        ↓
FPGA systolic array accelerator
```

The kernel driver owns the hardware-facing resources, such as mapped AXI-Lite registers and coherent DMA buffers. The user-space programs use `/dev/systolic_array` and ioctl calls to control the accelerator.

---

## 2. Repository Structure

```text
axis-systolic-array/
├── linux_driver/
│   ├── Makefile
│   ├── README.md
│   ├── systolic_array_main.c
│   ├── systolic_array_priv.h
│   ├── systolic_array_hw.c
│   ├── systolic_array_hw.h
│   ├── systolic_array_dma.c
│   ├── systolic_array_dma.h
│   ├── systolic_array_ioctl.h
│   ├── systolic_array_regs.h
│   └── systolic_array.dts.example
│
└── linux_test/
    ├── Makefile
    ├── README.md
    ├── reg_test.c
    ├── ioctl_test.c
    ├── dma_info_test.c
    └── transfer_test.c
```

---

## 3. Driver Components

The `linux_driver/` folder contains the kernel module implementation.

The driver creates:

```text
/dev/systolic_array
```

and exposes both:

1. a simple text-based debug interface
2. a structured ioctl interface

---

### `Makefile`

Builds the Linux kernel module.

The final kernel module is:

```text
systolic_array_drv.ko
```

Internally, the module is built from:

```text
systolic_array_main.o
systolic_array_hw.o
systolic_array_dma.o
```

This Makefile connects the driver source files into one loadable kernel module.

Important note: this Makefile is intended for the target PYNQ Linux system or a proper cross-compilation setup with matching PYNQ kernel headers. It is not expected to build successfully inside normal WSL unless WSL has matching kernel headers.

---

### `systolic_array_main.c`

Main Linux driver entry point.

This file handles the Linux-facing part of the driver:

- platform driver registration
- device tree matching
- `probe()` and `remove()` functions
- `/dev/systolic_array` creation
- `open`, `read`, `write`, and `ioctl` file operations

Important functions:

| Function | Purpose |
|---|---|
| `sa_probe()` | Called when Linux finds the FPGA device |
| `sa_remove()` | Called when the driver is removed |
| `sa_open()` | Prepares `file->private_data` correctly |
| `sa_read_file()` | Debug status read |
| `sa_write_file()` | Debug register write |
| `sa_ioctl()` | Handles structured user-space commands |

This file is where Linux connects the hardware device to the driver. It binds to the device tree node using the compatible string:

```text
custom,systolic-array
```

It also exposes the user-space interface:

```bash
cat /dev/systolic_array
echo "4 12345678" | sudo tee /dev/systolic_array
ioctl(fd, ...)
```

The `open()` function is important because for a `miscdevice`, `file->private_data` initially points to a `struct miscdevice`. The driver converts it into:

```c
struct sa_dev *
```

so that `read`, `write`, and `ioctl` all access the same internal driver state.

---

### `systolic_array_priv.h`

Private internal driver header.

This file defines the driver’s internal data structures:

```c
struct sa_dev
struct sa_dma_buffer
```

`struct sa_dev` stores:

- device pointer
- mapped AXI-Lite register base
- misc device object
- MM2S DMA buffers
- S2MM DMA buffer

This file is only for kernel driver implementation files. User-space tests should not include this file.

In short:

```text
systolic_array_priv.h = internal driver memory/state definition
```

---

### `systolic_array_hw.c`

Hardware register helper implementation.

This file handles low-level AXI-Lite register operations:

| Function | Purpose |
|---|---|
| `sa_hw_read_reg()` | Read one AXI-Lite register |
| `sa_hw_write_reg()` | Write one AXI-Lite register |
| `sa_hw_start()` | Start the accelerator |
| `sa_hw_done_status()` | Check done bits |
| `sa_hw_wait_done()` | Wait for completion with timeout |

This keeps hardware register logic separate from the Linux interface logic in `systolic_array_main.c`.

For example, when user space calls `SA_IOC_START`, `systolic_array_main.c` calls:

```c
sa_hw_start(sa);
```

Then `systolic_array_hw.c` writes to the `A_START` register.

---

### `systolic_array_hw.h`

Header for `systolic_array_hw.c`.

It declares:

```c
sa_hw_read_reg()
sa_hw_write_reg()
sa_hw_start()
sa_hw_done_status()
sa_hw_wait_done()
```

---

### `systolic_array_dma.c`

DMA helper implementation.

This file manages DMA-safe memory buffers owned by the kernel driver.

It handles:

- allocating coherent DMA buffers
- freeing coherent DMA buffers
- selecting DMA buffers by channel
- copying user input into DMA buffers
- copying DMA output back to user space

Current DMA channels:

| Channel | Meaning |
|---|---|
| `MM2S0` | Input stream 0: memory to hardware |
| `MM2S1` | Input stream 1: memory to hardware |
| `MM2S2` | Input stream 2: memory to hardware |
| `S2MM` | Output stream: hardware to memory |

Meaning:

```text
MM2S = memory-mapped to stream = input data from memory to hardware
S2MM = stream to memory-mapped = output data from hardware to memory
```

Conceptual data movement:

```text
User input array
    ↓ copy_from_user()
Driver MM2S DMA buffer
    ↓ FPGA reads it
Systolic array accelerator
    ↓ FPGA writes result
Driver S2MM DMA buffer
    ↓ copy_to_user()
User output array
```

This file is important because Linux user-space pointers are virtual addresses, but FPGA DMA needs DMA-safe addresses. The driver uses `dma_alloc_coherent()` to allocate memory that both the CPU and hardware can safely access.

---

### `systolic_array_dma.h`

Header for `systolic_array_dma.c`.

It declares:

```c
sa_dma_init()
sa_dma_cleanup()
sa_dma_get_buffer()
sa_dma_copy_to_buffer()
sa_dma_copy_from_buffer()
```

`systolic_array_main.c` calls these functions when processing DMA-related ioctl commands.

---

### `systolic_array_ioctl.h`

Shared user/kernel interface header.

This file is included by both:

- `linux_driver/`
- `linux_test/`

It defines the commands that user-space can send to the driver.

Current ioctl commands:

| Command | Purpose |
|---|---|
| `SA_IOC_READ_REG` | Read one AXI-Lite register |
| `SA_IOC_WRITE_REG` | Write one AXI-Lite register |
| `SA_IOC_START` | Start accelerator |
| `SA_IOC_WAIT` | Wait until done |
| `SA_IOC_GET_DMA_INFO` | Get allocated DMA buffer addresses |
| `SA_IOC_COPY_TO_DMA` | Copy user data into driver DMA buffer |
| `SA_IOC_COPY_FROM_DMA` | Copy output data from driver DMA buffer |
| `SA_IOC_RUN` | Program DMA registers, start hardware, and wait |

Shared structs:

| Struct | Purpose |
|---|---|
| `struct sa_reg_access` | Register index/value pair |
| `struct sa_dma_info` | DMA address information |
| `struct sa_dma_transfer` | Copy-to/from-DMA request |
| `struct sa_run_config` | Byte counts and timeout for running hardware |

This file is the contract between user space and kernel space.

---

### `systolic_array_regs.h`

Hardware register map.

This file defines register indices such as:

```c
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
```

Important idea:

```text
register index 4 is not byte address 4
byte offset = register index * REG_STRIDE_BYTES
```

For example, if each register is 32 bits:

```text
register index 4 → byte offset 16 → 0x10
```

---

### `systolic_array.dts.example`

Example device tree node.

Linux uses the device tree to know:

- what hardware exists
- where the register space is located
- which driver should bind to the hardware

Example:

```dts
systolic_array@40000000 {
    compatible = "custom,systolic-array";
    reg = <0x40000000 0x10000>;
};
```

The `compatible` string must match the driver’s device tree match table.

The address `0x40000000` is the expected AXI-Lite register base address for the accelerator.

---

## 4. User-Space Test Components

The `linux_test/` folder contains user-space C programs.

These are not kernel code. They run like normal Linux programs and validate the driver step by step.

---

### `linux_test/Makefile`

Builds all user-space test programs.

Current targets:

```text
reg_test
ioctl_test
dma_info_test
transfer_test
```

The Makefile includes headers from:

```text
../linux_driver
```

because the test programs need:

```text
systolic_array_regs.h
systolic_array_ioctl.h
```

Build command:

```bash
cd linux_test
make clean
make
```

In WSL/Docker, this should work and confirms that the user-space test code compiles.

---

### `reg_test.c`

Lowest-level test.

It does not use the custom kernel driver.

Instead, it opens:

```text
/dev/mem
```

and directly maps the FPGA register physical address into user space.

Purpose:

- check if FPGA bitstream is loaded
- check if AXI-Lite base address is correct
- check if raw register access works
- debug hardware before using the kernel driver

Run on PYNQ Linux:

```bash
sudo ./reg_test
```

This test requires `sudo` because `/dev/mem` gives direct physical memory access.

This test does not require `/dev/systolic_array`, but it does require the FPGA hardware to exist at the expected physical address.

---

### `ioctl_test.c`

Tests ioctl-based register access through the kernel driver.

It opens:

```text
/dev/systolic_array
```

Then sends:

```text
SA_IOC_WRITE_REG
SA_IOC_READ_REG
```

Purpose:

- check that `/dev/systolic_array` exists
- check that ioctl communication works
- check that the driver can read/write AXI-Lite registers

Run on PYNQ Linux:

```bash
sudo ./ioctl_test
```

Expected style of output:

```text
Register 4 = 0x12345678
```

This test should be run after the kernel driver is loaded.

---

### `dma_info_test.c`

Checks whether the driver successfully allocated DMA buffers.

It opens:

```text
/dev/systolic_array
```

Then calls:

```text
SA_IOC_GET_DMA_INFO
```

Purpose:

- check that DMA buffers exist
- check that the driver can return DMA metadata
- check that MM2S/S2MM channels are allocated

Expected output format:

```text
MM2S0 DMA address: 0x...
MM2S1 DMA address: 0x...
MM2S2 DMA address: 0x...
S2MM  DMA address: 0x...
Buffer size      : 4096 bytes
```

This does not run the accelerator. It only confirms the driver’s DMA allocation setup.

---

### `transfer_test.c`

Most complete user-space C test.

It tests the full software transfer API:

```text
open /dev/systolic_array
prepare input arrays
copy input arrays to MM2S DMA buffers
call SA_IOC_RUN
copy output from S2MM DMA buffer
print output words
```

It uses:

```text
SA_IOC_COPY_TO_DMA
SA_IOC_RUN
SA_IOC_COPY_FROM_DMA
```

Purpose:

- validate user-space to driver DMA copy
- validate driver programs DMA registers
- validate accelerator run command
- validate output copy back to user space
- prepare for Python/PYNQ wrapper

This is the C version of what the future Python wrapper will do.

---

## 5. How Components Match Together

### Register Access Path

```text
ioctl_test.c
    ↓ SA_IOC_WRITE_REG / SA_IOC_READ_REG
systolic_array_ioctl.h
    ↓ command definitions
systolic_array_main.c
    ↓ sa_ioctl()
systolic_array_hw.c
    ↓ sa_hw_write_reg() / sa_hw_read_reg()
systolic_array_regs.h
    ↓ register index
FPGA AXI-Lite register
```

---

### Start/Wait Path

```text
transfer_test.c or ioctl_test.c
    ↓ SA_IOC_START / SA_IOC_WAIT / SA_IOC_RUN
systolic_array_main.c
    ↓ sa_ioctl()
systolic_array_hw.c
    ↓ sa_hw_start()
    ↓ sa_hw_wait_done()
systolic_array_regs.h
    ↓ A_START and DONE registers
FPGA control registers
```

---

### DMA Info Path

```text
dma_info_test.c
    ↓ SA_IOC_GET_DMA_INFO
systolic_array_main.c
    ↓ fills struct sa_dma_info
systolic_array_dma.c
    ↓ buffers allocated during probe
DMA buffer addresses returned to user
```

---

### Full Transfer Path

```text
transfer_test.c
    ↓ SA_IOC_COPY_TO_DMA
systolic_array_main.c
    ↓ sa_ioctl()
systolic_array_dma.c
    ↓ copy_from_user()
MM2S DMA buffers

transfer_test.c
    ↓ SA_IOC_RUN
systolic_array_main.c
    ↓ program DMA registers
systolic_array_hw.c
    ↓ start/wait
FPGA accelerator runs

transfer_test.c
    ↓ SA_IOC_COPY_FROM_DMA
systolic_array_dma.c
    ↓ copy_to_user()
user output array
```

---

## 6. Board-Side Validation Order

When moving to PYNQ Linux, test in this order:

1. Load FPGA bitstream.
2. Run `reg_test`.
3. Build and insert kernel module.
4. Check `/dev/systolic_array`.
5. Run `ioctl_test`.
6. Run `dma_info_test`.
7. Run `transfer_test`.
8. Add and test Python/PYNQ wrapper.

Commands:

```bash
cd linux_driver
make
sudo insmod systolic_array_drv.ko
dmesg | tail
ls -l /dev/systolic_array
cat /dev/systolic_array

cd ../linux_test
make clean
make

sudo ./reg_test
sudo ./ioctl_test
sudo ./dma_info_test
sudo ./transfer_test
```

---

## 7. What Can Be Tested in WSL/Docker

In WSL/Docker, the kernel module itself is not expected to build or load because WSL does not have the target PYNQ kernel headers.

However, the user-space tests can be compiled:

```bash
cd /work/axis-systolic-array/linux_test
make clean
make
```

Expected binaries:

```text
reg_test
ioctl_test
dma_info_test
transfer_test
```

The Verilator smoke test can also be run:

```bash
cd /work/axis-systolic-array
make veri_smoke
```

Expected result:

```text
SMOKE TEST PASSED
```

---

## 8. Clock Skew Warning

In Docker/WSL, `make` may print:

```text
Clock skew detected. Your build may be incomplete.
```

This usually means some file timestamps are slightly ahead of the container clock. It is not necessarily a functional error.

To reduce the warning, run this from the repo root:

```bash
find . -exec touch {} +
```

Then rebuild:

```bash
cd linux_test
make clean
make
```

---

## 9. Completed Work

Completed driver/test features:

1. ioctl interface instead of only string-based debug writes
2. start/wait helper functions
3. coherent DMA buffer allocation
4. full input/output DMA transfer ioctl support
5. user-space C tests for each driver stage
6. README documentation for driver/test workflow

Remaining major item:

```text
Python/PYNQ wrapper
```

The Python/PYNQ wrapper should call the same ioctl interface that `transfer_test.c` already uses.
