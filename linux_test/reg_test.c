/*
 * User-space /dev/mem register smoke test for the AXI systolic array.
 *
 * This test bypasses the kernel driver and directly maps the AXI-Lite
 * register base address from user space. Use this before testing the
 * kernel driver to confirm that:
 *
 *   1. The FPGA bitstream is loaded.
 *   2. The AXI-Lite base address is correct.
 *   3. Linux can access the register space.
 *
 * For PYNQ-Z2, the generated config.py target uses:
 *
 *   CONFIG_BASE_PHYS = 0x40000000
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>

#include "systolic_array_regs.h"

#define MAP_SIZE 0x1000UL
#define MAP_MASK (MAP_SIZE - 1)

/* AXI-Lite base address generated for TARGET=pynq_z2 */
#define CONFIG_BASE_PHYS 0x40000000UL

static inline off_t reg_offset_bytes(uint32_t reg_index)
{
    return reg_index * REG_STRIDE_BYTES;
}

static inline uint32_t reg_read(volatile uint8_t *base, uint32_t reg_index)
{
    return *(volatile uint32_t *)(base + reg_offset_bytes(reg_index));
}

static inline void reg_write(volatile uint8_t *base,
                             uint32_t reg_index,
                             uint32_t value)
{
    *(volatile uint32_t *)(base + reg_offset_bytes(reg_index)) = value;
}

int main(void)
{
    int fd;
    void *map_base;
    volatile uint8_t *regs;

    off_t target = CONFIG_BASE_PHYS;
    off_t page_base = target & ~MAP_MASK;
    off_t page_offset = target & MAP_MASK;

    printf("Opening /dev/mem...\n");

    fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd < 0) {
        fprintf(stderr, "Failed to open /dev/mem: %s\n", strerror(errno));
        return 1;
    }

    printf("Mapping physical address 0x%08lx...\n",
           (unsigned long)target);

    map_base = mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE,
                    MAP_SHARED, fd, page_base);
    if (map_base == MAP_FAILED) {
        fprintf(stderr, "mmap failed: %s\n", strerror(errno));
        close(fd);
        return 1;
    }

    regs = (volatile uint8_t *)map_base + page_offset;

    printf("Initial register dump:\n");
    printf("  START       = 0x%08x\n", reg_read(regs, A_START));
    printf("  MM2S_0_DONE = 0x%08x\n", reg_read(regs, A_MM2S_0_DONE));
    printf("  MM2S_1_DONE = 0x%08x\n", reg_read(regs, A_MM2S_1_DONE));
    printf("  MM2S_2_DONE = 0x%08x\n", reg_read(regs, A_MM2S_2_DONE));
    printf("  S2MM_DONE   = 0x%08x\n", reg_read(regs, A_S2MM_DONE));

    /*
     * Safe first smoke test:
     * Use a configuration-like register instead of START.
     * This checks whether writes and readbacks work.
     */
    uint32_t old_val = reg_read(regs, A_MM2S_0_TUSER);
    uint32_t test_val = 0x12345678;
    uint32_t new_val;

    printf("Old MM2S_0_TUSER   = 0x%08x\n", old_val);
    printf("Writing test value = 0x%08x\n", test_val);

    reg_write(regs, A_MM2S_0_TUSER, test_val);

    new_val = reg_read(regs, A_MM2S_0_TUSER);
    printf("Readback value     = 0x%08x\n", new_val);

    if (new_val == test_val)
        printf("Register read/write smoke test PASSED\n");
    else
        printf("Register read/write smoke test FAILED\n");

    /*
     * Restore original value so the test does not leave the hardware
     * in a modified debug state.
     */
    reg_write(regs, A_MM2S_0_TUSER, old_val);
    printf("Restored value     = 0x%08x\n",
           reg_read(regs, A_MM2S_0_TUSER));

    if (munmap(map_base, MAP_SIZE) != 0)
        fprintf(stderr, "munmap failed: %s\n", strerror(errno));

    close(fd);

    return 0;
}