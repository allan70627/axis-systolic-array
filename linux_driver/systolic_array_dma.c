#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/errno.h>
#include <linux/gfp.h>

#include "systolic_array_priv.h"
#include "systolic_array_dma.h"

static int sa_dma_alloc_one(struct sa_dev *sa,
                            struct sa_dma_buffer *buf,
                            size_t size,
                            const char *name)
{
    buf->size = size;

    buf->virt = dma_alloc_coherent(sa->dev,
                                   buf->size,
                                   &buf->dma,
                                   GFP_KERNEL);
    if (!buf->virt) {
        dev_err(sa->dev, "failed to allocate %s DMA buffer\n", name);
        return -ENOMEM;
    }

    dev_info(sa->dev,
             "%s DMA buffer allocated: dma=%pad size=%zu\n",
             name,
             &buf->dma,
             buf->size);

    return 0;
}

static void sa_dma_free_one(struct sa_dev *sa, struct sa_dma_buffer *buf)
{
    if (!buf->virt)
        return;

    dma_free_coherent(sa->dev,
                      buf->size,
                      buf->virt,
                      buf->dma);

    buf->virt = NULL;
    buf->dma = 0;
    buf->size = 0;
}

int sa_dma_init(struct sa_dev *sa)
{
    int ret;

    ret = dma_set_mask_and_coherent(sa->dev, DMA_BIT_MASK(32));
    if (ret) {
        dev_err(sa->dev, "failed to set 32-bit coherent DMA mask\n");
        return ret;
    }

    ret = sa_dma_alloc_one(sa, &sa->in_buf, SA_DMA_BUF_SIZE, "input");
    if (ret)
        return ret;

    ret = sa_dma_alloc_one(sa, &sa->out_buf, SA_DMA_BUF_SIZE, "output");
    if (ret) {
        sa_dma_free_one(sa, &sa->in_buf);
        return ret;
    }

    return 0;
}

void sa_dma_cleanup(struct sa_dev *sa)
{
    sa_dma_free_one(sa, &sa->out_buf);
    sa_dma_free_one(sa, &sa->in_buf);
}