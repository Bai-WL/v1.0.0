/**
 * @file bsp_fifo.c
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.2
 * @date 2025-06-17
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include <stdlib.h>
#include <string.h>
#include "bsp_types.h"
#include "bsp_fifo.h"
#include "bsp_assert.h"

static inline uint32_t min(uint32_t _a, uint32_t _b) {
    return (_a > _b ? _b:_a);
}

//static inline uint32_t max(uint32_t _a, uint32_t _b) {
//    return (_a < _b ? _b:_a);
//}

/**
 * @brief 缓冲区大小必须为2的幂 
 * 
 * @param _fifo 
 * @param _size 
 * @return BSP_StatusTypedef 
 */
BSP_StatusTypedef bsp_fifo_init(struct bsp_fifo *_fifo, uint8_t *buffer, uint32_t _size) 
{
    if (NULL != _fifo->buffer) /* 避免重复初始化 */
        return BSP_ERROR;

    bsp_assert(!(_size & (_size - 1)));   /* 缓冲区大小必须为2的幂 */ 

    _fifo->buffer = buffer;
    _fifo->size   = _size;
    _fifo->out   = 0;
    _fifo->in   = 0;
    
    return BSP_OK;
}

/**
 * @brief 
 * 
 * @param _fifo 
 * @param _data 
 * @param _len 
 * @return * uint32_t 
 */
uint32_t bsp_fifo_put(struct bsp_fifo *_fifo, const uint8_t *_data, uint32_t _len) 
{
    uint32_t l;

    _len = min (_len, _fifo->size - _fifo->in + _fifo->out);

    l = min(_len, _fifo->size - (_fifo->in & (_fifo->size - 1))); /* 对头尾指针取余 */

    memcpy(_fifo->buffer + (_fifo->in & (_fifo->size - 1)), _data, l);
    memcpy(_fifo->buffer, _data + l, _len - l);

    _fifo->in += _len;

    return _len;
}

/**
 * @brief 
 * 
 * @param _fifo 
 * @param _data 
 * @param _len 
 * @return uint32_t 
 */
uint32_t bsp_fifo_get(struct bsp_fifo *_fifo, uint8_t *_data, uint32_t _len) 
{
    uint32_t l;

    _len = min(_len, _fifo->in - _fifo->out);
    l = min(_len, _fifo->size - (_fifo->out & (_fifo->size - 1)));
    
    memcpy(_data, _fifo->buffer + (_fifo->out & (_fifo->size - 1)), l);
    memcpy(_data + l, _fifo->buffer, _len - l);

    _fifo->out += _len;

    return _len;
}

/**
 * @brief 
 * 
 * @param _fifo 
 * @return uint32_t 
 */
uint32_t bsp_fifo_len(struct bsp_fifo *_fifo)
{
    return _fifo->in - _fifo->out;
}
