/**
 * @file bsp_malloc.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2025-03-06
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef __BSP_MALLOC_H__
#define __BSP_MALLOC_H__
#include <stdlib.h>
#include <stddef.h>
#ifdef USE_RTOS
#include "FreeRTOS.h"
#include "portable.h"
#endif

#ifdef USE_RTOS 
#define bsp_malloc(_e) pvPortMalloc(_e)
#define bsp_free(p) do { \
        vPortFree((void*)p); \
        p = NULL;           \
    } while(0)
#else 
#define bsp_malloc(_e) malloc(_e)
#define bsp_free(p) do { \
        free((void*)p);    \
        p = NULL;   \
    } while(0)  
#endif

#endif /* __BSP_MALLOC_H__ */ 
