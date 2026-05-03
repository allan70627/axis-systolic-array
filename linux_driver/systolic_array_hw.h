#ifndef SYSTOLIC_ARRAY_HW_H
#define SYSTOLIC_ARRAY_HW_H

#include <linux/types.h>
#include <linux/io.h>

struct sa_dev;

void sa_hw_start(struct sa_dev *sa);
u32 sa_hw_done_status(struct sa_dev *sa);
int sa_hw_wait_done(struct sa_dev *sa, unsigned int timeout_us);

u32 sa_hw_read_reg(struct sa_dev *sa, u32 reg_index);
void sa_hw_write_reg(struct sa_dev *sa, u32 reg_index, u32 value);

#endif