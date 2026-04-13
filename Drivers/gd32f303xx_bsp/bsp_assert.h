/**
 * @file bsp_assert.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2025-05-23
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef __BSP_DEBUG_H__
#define __BSP_DEBUG_H__
#ifdef BSP_DEBUG
#define bsp_assert(__e) ((__e) ? (void)0 : __bsp_assert_func__ (__FILE__, __LINE__))
#else 
#define bsp_assert(__e) ((void)0)
#endif /* BSP_DEBUG */
void __bsp_assert_func__ (const char *, int);
#endif /* __BSP_DEBUG_H__ */
