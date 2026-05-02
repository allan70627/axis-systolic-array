#ifndef SYSTOLIC_ARRAY_REGS_H
#define SYSTOLIC_ARRAY_REGS_H

/*
 * Systolic array register map.
 *
 * These values are register indices, not byte offsets.
 * The AXI-Lite register interface uses 32-bit registers, so the driver
 * converts each register index into a byte offset using:
 *
 *   byte_offset = register_index * REG_STRIDE_BYTES
 *
 * Example:
 *   A_MM2S_0_TUSER  = 0x4
 *   byte offset     = 0x4 * 4 = 0x10
 */

/* Global control register */
#define A_START         0x0

/* Memory-mapped-to-stream DMA channel 0 */
#define A_MM2S_0_DONE   0x1
#define A_MM2S_0_ADDR   0x2
#define A_MM2S_0_BYTES  0x3
#define A_MM2S_0_TUSER  0x4

/* Memory-mapped-to-stream DMA channel 1 */
#define A_MM2S_1_DONE   0x5
#define A_MM2S_1_ADDR   0x6
#define A_MM2S_1_BYTES  0x7
#define A_MM2S_1_TUSER  0x8

/* Memory-mapped-to-stream DMA channel 2 */
#define A_MM2S_2_DONE   0x9
#define A_MM2S_2_ADDR   0xA
#define A_MM2S_2_BYTES  0xB
#define A_MM2S_2_TUSER  0xC

/* Stream-to-memory-mapped DMA output channel */
#define A_S2MM_DONE     0xD
#define A_S2MM_ADDR     0xE
#define A_S2MM_BYTES    0xF

/* Largest valid register index in the current register map */
#define SA_MAX_REG_INDEX A_S2MM_BYTES

/* AXI-Lite registers are 32-bit wide */
#define REG_STRIDE_BYTES 4

#endif /* SYSTOLIC_ARRAY_REGS_H */