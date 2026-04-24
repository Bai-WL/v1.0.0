/**
 * @file key_control_example.c
 * @brief 按键控制系统使用示例
 * @version 1.0
 * @date 2026-04-18
 *
 * 说明：
 * 1. 展示如何初始化按键控制系统
 * 2. 展示如何与菜单系统集成
 * 3. 展示如何在主循环中处理按键事件
 * 4. 提供配置示例和最佳实践
 *
 * 注意：这只是一个示例，实际使用时需要根据项目需求调整
 */

#include "bsp_gd32f303xx_systick.h"
#include "key_control.h"
#include "menu_system.h"

// ============================================================================
// 示例1：基本初始化
// ============================================================================

/**
 * @brief 示例1：基本初始化和简单事件处理
 *
 * 这个示例展示了最基本的初始化步骤和事件循环
 */
void example1_basic_init(void) {
    // 1. 初始化系统滴答（如果还未初始化）
    // bsp_systick_init(); // 通常在main.c中已经初始化

    // 2. 初始化按键控制系统
    key_control_init();

    // 3. 可以自定义按键配置（可选）
    KeyConfig custom_config = {
        .debounce_time = 20,      // 20ms消抖
        .long_press_time = 1200,  // 1.2秒长按
        .repeat_delay = 600,      // 600ms后开始重复
        .repeat_interval = 150    // 150ms重复间隔
    };
    key_control_set_config(&custom_config);

    // 4. 可以自定义按键映射（可选）
    // 注意：这里展示了如何重新映射按键
    FunKeyMapping custom_mapping[6] = {
        FUNKEY_UP,     // F1 -> UP（默认）
        FUNKEY_DOWN,   // F2 -> DOWN（默认）
        FUNKEY_ENTER,  // F3 -> ENTER（重新映射）
        FUNKEY_ESC,    // F4 -> ESC（重新映射）
        FUNKEY_LEFT,   // F5 -> LEFT（重新映射）
        FUNKEY_RIGHT   // F6 -> RIGHT（重新映射）
    };
    key_control_set_key_mapping(custom_mapping);
}

/**
 * @brief 示例1的事件处理循环
 *
 * 这个函数应该在主循环中定期调用
 */
void example1_event_loop(void) {
    uint32_t current_tick = bsp_get_systick();

    // 1. 扫描按键
    key_control_scan(current_tick);

    // 2. 处理所有按键事件
    KeyEvent event;
    while (key_control_get_event(&event)) {
        // 处理按键事件
        switch (event.key) {
        case FUNKEY_UP:
            // 处理上键
            break;

        case FUNKEY_DOWN:
            // 处理下键
            break;

        case FUNKEY_ENTER:
            // 处理确认键
            break;

        case FUNKEY_ESC:
            // 处理返回键
            break;

        case FUNKEY_LEFT:
            // 处理左键
            break;

        case FUNKEY_RIGHT:
            // 处理右键
            break;

        default:
            break;
        }
    }
}

// ============================================================================
// 示例2：与菜单系统集成
// ============================================================================

/**
 * @brief 示例2：与菜单系统的完整集成
 *
 * 这个示例展示了如何将按键控制系统与现有的菜单系统集成
 */
void example2_menu_integration(void) {
    // 1. 初始化按键控制系统
    key_control_init();

    // 2. 初始化菜单系统
    menu_system_init();

    // 3. 设置按键事件回调到菜单系统
    key_control_set_menu_callback(menu_handle_key_event);

    // 4. 加载菜单配置（示例）
    // extern const MenuItem menu_items[];
    // extern const uint16_t menu_item_count;
    // menu_load_config(menu_items, menu_item_count);

    // 5. 启动菜单系统
    // menu_start(ROOT_MENU_ID);
}

/**
 * @brief 示例2的完整事件循环
 *
 * 这个函数展示了完整的集成处理流程
 */
void example2_main_loop(void) {
    static uint32_t last_tick = 0;
    uint32_t current_tick = bsp_get_systick();

    // 1. 每1ms执行一次按键扫描（可以在定时中断中执行）
    key_control_scan_simple();

    // 2. 每100ms处理一次菜单逻辑（主循环中）
    if (current_tick - last_tick >= 100) {
        // 2.1 处理按键事件（如果使用了回调，事件会被自动处理）
        // 如果没有使用回调，可以手动处理：
        // KeyEvent event;
        // while (key_control_get_event(&event)) {
        //     menu_handle_key_event(event.key, event.type);
        // }

        // 2.2 处理菜单定时器事件
        menu_handle_timer(current_tick);

        // 2.3 其他100ms任务...

        last_tick = current_tick;
    }
}

// ============================================================================
// 示例3：高级功能配置
// ============================================================================

/**
 * @brief 示例3：高级功能配置
 *
 * 这个示例展示了如何使用按键控制系统的高级功能
 */
void example3_advanced_features(void) {
    // 1. 初始化
    key_control_init();

    // 2. 禁用长按功能（只支持短按）
    key_control_enable_long_press(false);

    // 3. 禁用重复触发功能
    key_control_enable_repeat(false);

    // 4. 设置自定义重复间隔
    key_control_set_auto_repeat_interval(200);  // 200ms重复间隔

    // 5. 查询按键状态
    bool is_up_pressed = key_control_is_key_pressed(FUNKEY_UP);
    uint32_t press_time = key_control_get_key_press_time(FUNKEY_DOWN);

    // 6. 事件类型和按键名称转换（调试用途）
    const char* event_str = key_event_type_to_string(KEY_EVENT_PRESS);
    const char* key_str = key_mapping_to_string(FUNKEY_ENTER);

    // 7. 原始按键值映射
    uint8_t raw_key = 0x01;  // 示例：F1键
    FunKeyMapping mapped = key_map_raw_to_function(raw_key);
    // mapped 现在应该是 FUNKEY_UP（如果使用默认映射）
}

// ============================================================================
// 示例4：与现有代码的兼容性
// ============================================================================

/**
 * @brief 示例4：保持与现有代码的兼容性
 *
 * 这个示例展示了如何逐步迁移现有代码到新的按键控制系统
 */
void example4_backward_compatibility(void) {
    // 场景1：现有代码使用 hid_get_funkey() 获取原始按键值
    // 可以逐步替换为新的按键控制系统

    // 旧代码：
    // uint8_t raw_key = hid_get_funkey();
    // if (raw_key == 0x01) { // F1
    //     // 处理F1键
    // }

    // 新代码：
    key_control_init();
    KeyEvent event;
    if (key_control_get_event(&event)) {
        if (event.key == FUNKEY_UP) {  // 映射后的F1
            // 处理上键
        }
    }

    // 场景2：现有代码在1ms定时中断中调用 hid_scan()
    // 可以替换为 key_control_scan_simple()

    // 旧代码（在1ms中断中）：
    // void TIMER1_IRQHandler(void) {
    //     hid_scan();
    // }

    // 新代码（在1ms中断中）：
    // void TIMER1_IRQHandler(void) {
    //     key_control_scan_simple();
    // }

    // 或者更好的方式：在主循环中处理
}

// ============================================================================
// 最佳实践总结
// ============================================================================

/**
 * @brief 按键控制系统最佳实践总结
 *
 * 1. **初始化顺序**：
 *    - 先初始化系统滴答
 *    - 再初始化按键控制系统
 *    - 最后初始化菜单系统
 *
 * 2. **事件处理**：
 *    - 在1ms定时中断中调用 key_control_scan_simple()
 *    - 在主循环的100ms任务中处理事件
 *    - 使用回调函数简化集成
 *
 * 3. **配置调整**：
 *    - 根据用户体验调整消抖时间（15-25ms）
 *    - 根据需求调整长按时间（800-1500ms）
 *    - 根据操作习惯调整重复间隔（50-200ms）
 *
 * 4. **调试技巧**：
 *    - 使用 key_event_type_to_string() 和 key_mapping_to_string() 调试
 *    - 检查事件队列是否溢出
 *    - 验证按键映射是否正确
 *
 * 5. **电源管理**：
 *    - 在休眠前保存按键状态
 *    - 唤醒后恢复按键配置
 *    - 考虑低功耗模式下的按键响应
 */

// ============================================================================
// 示例：完整的主函数集成
// ============================================================================

/**
 * @brief 示例：完整的main函数集成
 *
 * 注意：这是一个完整的示例，实际项目可能需要调整
 */
void example_complete_main_integration(void) {
    // 系统初始化（通常在main.c的main函数开头）

    // 1. 硬件初始化
    // system_clock_init();
    // gpio_init();
    // bsp_systick_init();

    // 2. 外设初始化
    // hid_init(); // HID驱动初始化
    // bsp_JLXLcdInit(); // 显示驱动初始化

    // 3. 软件模块初始化
    key_control_init();  // 按键控制系统初始化
    menu_system_init();  // 菜单系统初始化

    // 4. 配置集成
    key_control_set_menu_callback(menu_handle_key_event);  // 设置回调

    // 5. 加载菜单配置
    // menu_load_config(g_menu_items, MENU_ITEM_COUNT);
    // menu_set_language(LANG_ZH_CN);
    // menu_start(ROOT_MENU_ID);

    // 主循环（示例）
    while (1) {
        uint32_t current_tick = bsp_get_systick();
        static uint32_t last_100ms_tick = 0;

        // A. 按键扫描（可以在1ms中断中执行）
        key_control_scan(current_tick);

        // B. 100ms任务
        if (current_tick - last_100ms_tick >= 100) {
            // 1. 处理菜单定时器
            menu_handle_timer(current_tick);

            // 2. 其他100ms任务...

            last_100ms_tick = current_tick;
        }

        // C. 其他任务...
        // - 通信处理
        // - 数据处理
        // - 状态监控

        // D. 低功耗管理（如果需要）
        // if (system_is_idle()) {
        //     enter_low_power_mode();
        // }
    }
}

// ============================================================================
// 常见问题解答
// ============================================================================

/**
 * @brief 常见问题解答
 *
 * Q1: 按键响应延迟怎么办？
 * A1: 检查消抖时间设置，15-20ms通常足够。
 *     确保 key_control_scan() 被频繁调用（至少每1ms）。
 *
 * Q2: 长按功能不工作？
 * A2: 检查 key_control_enable_long_press(true) 是否被调用。
 *     检查 long_press_time 配置是否合理。
 *
 * Q3: 重复触发太快/太慢？
 * A3: 调整 repeat_interval 参数。
 *     考虑用户操作习惯，数值编辑时可能需要快速重复。
 *
 * Q4: 事件丢失怎么办？
 * A4: 增加 EVENT_QUEUE_SIZE。
 *     确保主循环及时处理事件。
 *     检查是否有长时间的阻塞操作。
 *
 * Q5: 如何调试按键事件？
 * A5: 使用 key_event_type_to_string() 和 key_mapping_to_string()。
 *     在事件处理中添加调试输出。
 *     检查原始按键值是否正确。
 */

// ============================================================================
// 迁移指南
// ============================================================================

/**
 * @brief 从现有系统迁移到按键控制系统的指南
 *
 * 步骤1：添加新文件
 *   - 将 key_control.h 和 key_control.c 添加到项目
 *   - 确保编译通过
 *
 * 步骤2：逐步替换
 *   - 先在现有代码中初始化 key_control_init()
 *   - 将 hid_scan() 替换为 key_control_scan_simple()
 *   - 逐步替换按键处理逻辑
 *
 * 步骤3：集成测试
 *   - 测试短按、长按、重复触发
 *   - 测试不同配置下的用户体验
 *   - 验证与菜单系统的集成
 *
 * 步骤4：优化调整
 *   - 根据测试结果调整配置参数
 *   - 优化事件处理流程
 *   - 添加必要的错误处理
 *
 * 步骤5：完全迁移
 *   - 移除旧的按键处理代码
 *   - 清理不用的函数和变量
 *   - 更新文档和注释
 */

// ============================================================================
// 注意事项
// ============================================================================

/**
 * @brief 重要注意事项
 *
 * 1. **线程安全**：
 *    如果系统有多任务，需要考虑事件队列的线程安全。
 *
 * 2. **中断上下文**：
 *    key_control_scan_simple() 可以在中断中调用，
 *    但 key_control_get_event() 和回调函数应在主循环中处理。
 *
 * 3. **资源占用**：
 *    按键控制系统占用约200字节RAM（可调整EVENT_QUEUE_SIZE）。
 *    代码占用约2KB Flash（取决于功能启用情况）。
 *
 * 4. **可配置性**：
 *    所有时间参数都可以运行时调整。
 *    按键映射可以动态改变。
 *
 * 5. **兼容性**：
 *    与现有 menu_system.h 的数据类型兼容。
 *    可以逐步迁移，不影响现有功能。
 */

// 文件结束
// 实际使用时应根据项目需求调整和优化