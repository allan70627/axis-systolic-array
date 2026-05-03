#ifndef SYSTOLIC_ARRAY_DMA_H
#define SYSTOLIC_ARRAY_DMA_H

#include <linux/types.h>

#include "systolic_array_ioctl.h"

struct sa_dev;
struct sa_dma_buffer;

int sa_dma_init(struct sa_dev *sa);
void sa_dma_cleanup(struct sa_dev *sa);

struct sa_dma_buffer *sa_dma_get_buffer(struct sa_dev *sa, sa_u32 channel);

int sa_dma_copy_to_buffer(struct sa_dev *sa,
                          const struct sa_dma_transfer *xfer);

int sa_dma_copy_from_buffer(struct sa_dev *sa,
                            const struct sa_dma_transfer *xfer);

#endif