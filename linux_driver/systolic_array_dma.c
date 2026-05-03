#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/errno.h>
#include <linux/gfp.h>
#include <linux/uaccess.h>

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
    int i;
    char name[16];

    ret = dma_set_mask_and_coherent(sa->dev, DMA_BIT_MASK(32));
    if (ret) {
        dev_err(sa->dev, "failed to set 32-bit coherent DMA mask\n");
        return ret;
    }

    for (i = 0; i < SA_NUM_MM2S; i++) {
        snprintf(name, sizeof(name), "mm2s%d", i);

        ret = sa_dma_alloc_one(sa, &sa->mm2s[i], SA_DMA_BUF_SIZE, name);
        if (ret)
            goto err_free_mm2s;
    }

    ret = sa_dma_alloc_one(sa, &sa->s2mm, SA_DMA_BUF_SIZE, "s2mm");
    if (ret)
        goto err_free_mm2s;

    return 0;

err_free_mm2s:
    while (i--)
        sa_dma_free_one(sa, &sa->mm2s[i]);

    return ret;
}

void sa_dma_cleanup(struct sa_dev *sa)
{
    int i;

    sa_dma_free_one(sa, &sa->s2mm);

    for (i = 0; i < SA_NUM_MM2S; i++)
        sa_dma_free_one(sa, &sa->mm2s[i]);
}

struct sa_dma_buffer *sa_dma_get_buffer(struct sa_dev *sa, sa_u32 channel)
{
    switch (channel) {
    case SA_DMA_CH_MM2S0:
        return &sa->mm2s[0];

    case SA_DMA_CH_MM2S1:
        return &sa->mm2s[1];

    case SA_DMA_CH_MM2S2:
        return &sa->mm2s[2];

    case SA_DMA_CH_S2MM:
        return &sa->s2mm;

    default:
        return NULL;
    }
}

static int sa_dma_check_range(struct sa_dma_buffer *buf,
                              sa_u32 offset,
                              sa_u32 num_bytes)
{
    if (!buf || !buf->virt)
        return -EINVAL;

    if (offset > buf->size)
        return -EINVAL;

    if (num_bytes > buf->size - offset)
        return -EINVAL;

    return 0;
}

int sa_dma_copy_to_buffer(struct sa_dev *sa,
                          const struct sa_dma_transfer *xfer)
{
    struct sa_dma_buffer *buf;
    void __user *user_ptr;
    int ret;

    buf = sa_dma_get_buffer(sa, xfer->channel);
    ret = sa_dma_check_range(buf, xfer->offset, xfer->num_bytes);
    if (ret)
        return ret;

    user_ptr = (void __user *)(unsigned long)xfer->user_ptr;

    if (copy_from_user(buf->virt + xfer->offset,
                       user_ptr,
                       xfer->num_bytes))
        return -EFAULT;

    return 0;
}

int sa_dma_copy_from_buffer(struct sa_dev *sa,
                            const struct sa_dma_transfer *xfer)
{
    struct sa_dma_buffer *buf;
    void __user *user_ptr;
    int ret;

    buf = sa_dma_get_buffer(sa, xfer->channel);
    ret = sa_dma_check_range(buf, xfer->offset, xfer->num_bytes);
    if (ret)
        return ret;

    user_ptr = (void __user *)(unsigned long)xfer->user_ptr;

    if (copy_to_user(user_ptr,
                     buf->virt + xfer->offset,
                     xfer->num_bytes))
        return -EFAULT;

    return 0;
}