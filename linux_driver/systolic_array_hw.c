#include <linux/delay.h>
#include <linux/errno.h>

#include "systolic_array_hw.h"
#include "systolic_array_regs.h"
#include "systolic_array_priv.h"

static inline void __iomem *sa_reg_addr(struct sa_dev *sa, u32 reg_index)
{
    return sa->regs + reg_index * REG_STRIDE_BYTES;
}

u32 sa_hw_read_reg(struct sa_dev *sa, u32 reg_index)
{
    return ioread32(sa_reg_addr(sa, reg_index));
}

void sa_hw_write_reg(struct sa_dev *sa, u32 reg_index, u32 value)
{
    iowrite32(value, sa_reg_addr(sa, reg_index));
}

void sa_hw_start(struct sa_dev *sa)
{
    sa_hw_write_reg(sa, A_START, 1);
}

u32 sa_hw_done_status(struct sa_dev *sa)
{
    /*
     * Return 1 only when all known DMA done bits are asserted.
     */
    return sa_hw_read_reg(sa, A_MM2S_0_DONE) &
           sa_hw_read_reg(sa, A_MM2S_1_DONE) &
           sa_hw_read_reg(sa, A_MM2S_2_DONE) &
           sa_hw_read_reg(sa, A_S2MM_DONE);
}

int sa_hw_wait_done(struct sa_dev *sa, unsigned int timeout_us)
{
    while (timeout_us--) {
        if (sa_hw_done_status(sa))
            return 0;

        udelay(1);
    }

    return -ETIMEDOUT;
}