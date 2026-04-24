#include <stdio.h>

#include "bsp.h"
#include "bsp_gd32f303xx_systick.h"
#include "bsp_gd32f30x_hhctl.h"
#include "bsp_modbus_tcp.h"
#include "drv_pm.h"
#include "gd32f30x_libopt.h"
#include "key_control.h"
#include "menu_system.h"
#include "poll_manager.h"
#include "user_mb_controller.h"
#include "user_poll.h"

static void menu_rs485_runtime_process(uint32_t current_tick) {
    static uint32_t last_poll_tick = 0;
    static uint32_t last_check_tick = 0;
    uint16_t changed_addrs[MAX_ADDR_PER_SCREEN + ALWAYS_POLL_ADDR_COUNT];
    uint32_t changed_values[MAX_ADDR_PER_SCREEN + ALWAYS_POLL_ADDR_COUNT];
    uint8_t changed_count;

    (void)current_tick;

    // 菜单轮询请求以较低频率生成，避免反复灌满普通请求队列。
    if ((current_tick - last_poll_tick) >= 100U) {
        last_poll_tick = current_tick;
        add_menu_polltask();
    }

    // 变化检测与UI刷新节拍略快一些，保证菜单显示及时跟随缓存更新。
    if ((current_tick - last_check_tick) < 50U) {
        return;
    }

    last_check_tick = current_tick;
    changed_count = PollManager_CheckAndUpdateValues(changed_addrs, changed_values);
    if (changed_count > 0U) {
        menu_handle_data_updates(changed_addrs, changed_values, changed_count);
        menu_request_redraw();
    }
}

int main(void) {
    uint32_t last_1ms_tick = 0;    // 1ms任务上次执行时间
    uint32_t last_10ms_tick = 0;   // 10ms任务上次执行时间
    uint32_t last_100ms_tick = 0;  // 100ms任务上次执行时间

    // 硬件初始化
    bsp_hhctl_init();
    // 功能模块初始化
    key_control_init();
    MBController_Init();
    test_menu_navigation();  // 测试菜单导航功能
    // 版本特定初始化
#ifdef WIFI_H02W

#else
    // 有线版本初始化RS485 Modbus
#endif

    // 使能全局中断
//    __disable_irq();
    // printf("系统初始化完成\r\n");  // 注释掉以节省内存

    // 主循环 - 时间片任务模式
    while (1) {
        uint32_t current_tick = bsp_get_systick();

        // 1ms
        if (current_tick - last_1ms_tick >= 1) {
            last_1ms_tick = current_tick;
            key_menu_loop(last_1ms_tick);  // 集成按键扫描和菜单处理
        }

        // 10ms
        if (current_tick - last_10ms_tick >= 10) {
            last_10ms_tick = current_tick;
            menu_rs485_runtime_process(last_10ms_tick);
        }

        // 100ms
        if (current_tick - last_100ms_tick >= 100) {
            last_100ms_tick = current_tick;
            menu_handle_timer(last_100ms_tick);
        }
        // 有线通讯状态机

        MBController_Process(&hMBMaster);
        // 电源管理状态机
        pm_handler();

        // 低功耗等待
        __WFI();
    }
}
