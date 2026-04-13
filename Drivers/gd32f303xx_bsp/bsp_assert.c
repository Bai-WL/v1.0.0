/**
 * @file bsp_assert.c
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.2
 * @date 2025-06-17
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include <stdio.h>
char g_dbg_msg[256];
void __bsp_assert_func__(const char *_file, int _line)
{
    volatile char i = 0;
	sprintf(g_dbg_msg, "%s, %d", _file, _line);
    while (1) {
        i++;
    }
}
