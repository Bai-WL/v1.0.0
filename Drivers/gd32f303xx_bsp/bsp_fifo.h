/**
 * @file bsp_fifo.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2025-05-08
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef __BSP_FIFO_H__
#define __BSP_FIFO_H__
#include "stdint.h"
#include "stdlib.h"
#include "bsp_types.h"

struct bsp_fifo {
    uint8_t *buffer;
    uint32_t size;
    uint32_t out;
    uint32_t in;
};

static inline void bsp_fifo_free(struct bsp_fifo *fifo) {
    free(fifo->buffer);
}

BSP_StatusTypedef bsp_fifo_init(struct bsp_fifo *_fifo, uint8_t *buffer, uint32_t _size); 
uint32_t bsp_fifo_put(struct bsp_fifo *_fifo, const uint8_t *_data, uint32_t _len);
uint32_t bsp_fifo_get(struct bsp_fifo *_fifo, uint8_t *_data, uint32_t _len);
uint32_t bsp_fifo_len(struct bsp_fifo *_fifo);

#endif /* __BSP_FIFO_H__ */
