#include <stdio.h>

#include "bsp.h"
#include "bsp_gd32f303xx_systick.h"
#include "bsp_gd32f30x_hhctl.h"
#include "bsp_modbus_tcp.h"
#include "drv_pm.h"
#include "gd32f30x_libopt.h"
#include "menu_system.h"
#include "user_mb_controller.h"

int main(void) {
    uint32_t last_1ms_tick = 0;    // 1ms任务上次执行时间
    uint32_t last_10ms_tick = 0;   // 10ms任务上次执行时间
    uint32_t last_100ms_tick = 0;  // 100ms任务上次执行时间

    // 硬件初始化
    bsp_hhctl_init();
    test_menu_navigation();  // 测试菜单导航功能
    // 版本特定初始化
#ifdef WIFI_H02W

#else
    // 有线版本初始化RS485 Modbus
#endif

    // 使能全局中断
    __enable_irq();
    // printf("系统初始化完成\r\n");  // 注释掉以节省内存

    // 主循环 - 时间片任务模式
    while (1) {
        uint32_t current_tick = bsp_get_systick();

        // 1ms
        if (current_tick - last_1ms_tick >= 1) {
            last_1ms_tick = current_tick;
            bsp_RunPer1ms();
        }

        // 10ms
        if (current_tick - last_10ms_tick >= 10) {
            last_10ms_tick = current_tick;
            bsp_RunPer10ms();
        }

        // 100ms
        if (current_tick - last_100ms_tick >= 100) {
            last_100ms_tick = current_tick;
            menu_handle_timer(last_100ms_tick);
            bsp_JLXLcdRefreshScreen();  // 刷新屏幕显示
        }
        // 有线通讯状态机
        MBController_Process(&hMBMaster);
        // 电源管理状态机
        pm_handler();

        // 低功耗等待
        __WFI();
    }
}
