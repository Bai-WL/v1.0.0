#include <stdio.h>
#include "gd32f30x_libopt.h"
#include "bsp_gd32f30x_hhctl.h"
#include "bsp_gd32f303xx_systick.h"
#include "bsp_modbus_tcp.h"
#include "bsp.h"
#include "drv_pm.h"

int main(void)
{
    uint32_t last_1ms_tick = 0;   // 1ms任务上次执行时间
    uint32_t last_10ms_tick = 0;  // 10ms任务上次执行时间
    uint32_t last_100ms_tick = 0; // 100ms任务上次执行时间

    // 硬件初始化
    bsp_hhctl_init();

    // 版本特定初始化
#ifdef WIFI_H02W
    bsp_modbus_tcp_window_init(); // WiFi版本初始化TCP窗口
    printf("WiFi版本: 支持锂电池充电和TCP通信\r\n");
#else
    printf("有线版本: 外接电源供电\r\n");
#endif

    // 使能全局中断
    __enable_irq();
    printf("系统初始化完成\r\n");

    // 主循环 - 时间片任务模式
    while (1)
    {
        uint32_t current_tick = bsp_get_systick();

        // 1ms
        if (current_tick - last_1ms_tick >= 1)
        {
            last_1ms_tick = current_tick;
            bsp_RunPer1ms();
        }

        // 10ms
        if (current_tick - last_10ms_tick >= 10)
        {
            last_10ms_tick = current_tick;
            bsp_RunPer10ms();
        }

        // 100ms
        if (current_tick - last_100ms_tick >= 100)
        {
            last_100ms_tick = current_tick;
        }

        // 电源管理状态机
        pm_handler();

        // 低功耗等待
        __WFI();
    }
}
