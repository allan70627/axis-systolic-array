#ifndef SYSTOLIC_ARRAY_DMA_H
#define SYSTOLIC_ARRAY_DMA_H

struct sa_dev;

int sa_dma_init(struct sa_dev *sa);
void sa_dma_cleanup(struct sa_dev *sa);

#endif