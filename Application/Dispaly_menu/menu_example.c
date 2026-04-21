// menu_example.c
// 根据详细界面菜单项配置重新整理的示例菜单

#include <stddef.h>

#include "language_resources.h"
#include "menu_system.h"

#define COUNT_OF(array) ((uint8_t)(sizeof(array) / sizeof((array)[0])))

#define SUBMENU_ITEM(id, text, parent, first_child, next, target)                 \
    {                                                                             \
        MENU_ITEM_TYPE_SUBMENU, id, text, STR_NONE, 0, parent, first_child, next, \
            MENU_RS485_ADDR_NONE, MENU_TYPE_VALUE_UINT16, 0, .data = {            \
                .target_menu_id = target                                          \
            }                                                                     \
    }

#define ACTION_ITEM(id, text, parent, next, callback)                                        \
    {                                                                                        \
        MENU_ITEM_TYPE_ACTION, id, text, STR_NONE, 0, parent, 0, next, MENU_RS485_ADDR_NONE, \
            MENU_TYPE_VALUE_UINT16, 0, .data = {                                             \
                .action_callback = callback                                                  \
            }                                                                                \
    }

#define VALUE_ITEM(id, text, parent, next, addr, minv, maxv, dp)                      \
    {                                                                                 \
        MENU_ITEM_TYPE_VALUE, id, text, STR_NONE, 0, parent, 0, next, addr,           \
            MENU_TYPE_VALUE_UINT16, 2, {                                              \
            .value_ptr = NULL, .min_value = minv, .max_value = maxv, .step_value = 1, \
            .decimal_places = dp, .value_changed = NULL                               \
        }                                                                             \
    }

#define TOGGLE_ITEM(id, text, parent, next, addr, options)                                        \
    {                                                                                             \
        MENU_ITEM_TYPE_TOGGLE, id, text, STR_NONE, 0, parent, 0, next, addr, MENU_TYPE_VALUE_BIT, \
            1, {                                                                                  \
            .toggle_value = NULL, .toggle_option_ids = options,                                   \
            .toggle_option_count = COUNT_OF(options), .toggle_changed = NULL                      \
        }                                                                                         \
    }

#define LIST_ITEM_BIT(id, text, parent, next, addr, opts)                                          \
    {                                                                                              \
        MENU_ITEM_TYPE_LIST, id, text, STR_NONE, 0, parent, 0, next, addr, MENU_TYPE_VALUE_BIT, 1, \
            .data = {                                                                              \
                .selection_ptr = NULL,                                                             \
                .option_ids = opts,                                                                \
                .option_count = COUNT_OF(opts),                                                    \
                .selection_changed = NULL                                                          \
            }                                                                                      \
    }

#define LIST_ITEM_WORD(id, text, parent, next, addr, options)                                      \
    {                                                                                              \
        MENU_ITEM_TYPE_LIST, id, text, STR_NONE, 0, parent, 0, next, addr, MENU_TYPE_VALUE_UINT16, \
            2, {                                                                                   \
            .selection_ptr = NULL, .option_ids = options, .option_count = COUNT_OF(options),       \
            .selection_changed = NULL                                                              \
        }                                                                                          \
    }

#define MOMENTARY_ITEM(id, text, parent, next, addr, options)                      \
    {                                                                              \
        MENU_ITEM_TYPE_MOMENTARY, id, text, STR_NONE, 0, parent, 0, next, addr,    \
            MENU_TYPE_VALUE_BIT, 1, {                                              \
            .momentary_value = NULL, .momentary_option_ids = options,              \
            .momentary_option_count = COUNT_OF(options), .momentary_changed = NULL \
        }                                                                          \
    }

#define INFO_ITEM_WORD(id, text, parent, next, addr, dp)                                           \
    {                                                                                              \
        MENU_ITEM_TYPE_INFO, id, text, STR_NONE, 0, parent, 0, next, addr, MENU_TYPE_VALUE_UINT16, \
            2, {                                                                                   \
            .value_ptr = NULL, .min_value = 0, .max_value = 0, .step_value = 1,                    \
            .decimal_places = dp, .value_changed = NULL                                            \
        }                                                                                          \
    }

#define INFO_ITEM_DWORD(id, text, parent, next, addr, dp)                                          \
    {                                                                                              \
        MENU_ITEM_TYPE_INFO, id, text, STR_NONE, 0, parent, 0, next, addr, MENU_TYPE_VALUE_UINT32, \
            4, {                                                                                   \
            .value_ptr = NULL, .min_value = 0, .max_value = 0, .step_value = 1,                    \
            .decimal_places = dp, .value_changed = NULL                                            \
        }                                                                                          \
    }

static StringID yes_no_option_ids[] = {STR_NO, STR_YES};
static StringID dao_xin_option_ids[] = {STR_DAO, STR_XIN};
static StringID flash_mode_option_ids[] = {STR_FLASH, STR_STEADY_ON};
static StringID language_option_ids[] = {STR_CN, STR_EN};
static StringID normally_open_close_option_ids[] = {STR_NORMALLY_OPEN, STR_NORMALLY_CLOSED};
static StringID on_off_option_ids[] = {STR_CLOSE, STR_OPEN};

static const IOMonitorPoint io_monitor_x_points[] = {
    {STR_X0, 18176U},  {STR_X1, 18177U},  {STR_X2, 18178U},  {STR_X3, 18179U},
    {STR_X4, 18180U},  {STR_X5, 18181U},  {STR_X6, 18182U},  {STR_X7, 18183U},
    {STR_X8, 18184U},  {STR_X9, 18185U},  {STR_X10, 18186U}, {STR_X11, 18187U},
    {STR_X12, 18188U}, {STR_X13, 18189U}, {STR_X14, 18190U}, {STR_X15, 18191U},
};

static const IOMonitorPoint io_monitor_y_points[] = {
    {STR_Y0, 17920U},  {STR_Y1, 17921U},  {STR_Y2, 17922U},  {STR_Y3, 17923U},
    {STR_Y4, 17924U},  {STR_Y5, 17925U},  {STR_Y6, 17926U},  {STR_Y7, 17927U},
    {STR_Y8, 17928U},  {STR_Y9, 17929U},  {STR_Y10, 17930U}, {STR_Y11, 17931U},
    {STR_Y12, 17932U}, {STR_Y13, 17933U}, {STR_Y14, 17934U}, {STR_Y15, 17935U},
};

static const IOMonitorConfig io_monitor_config = {
    .x_points = io_monitor_x_points,
    .x_count = COUNT_OF(io_monitor_x_points),
    .y_points = io_monitor_y_points,
    .y_count = COUNT_OF(io_monitor_y_points),
};

static const AlarmLogConfig alarm_log_config = {
    .menu_id = 500U,
    .rs485_addr = 0U,  // 文档未给出报警码来源地址，这里保留占位地址，后续按实际PLC地址替换
    .rs485_type = MENU_TYPE_VALUE_UINT16,
};

static void open_io_monitor(void) {
    menu_open_io_monitor();
}

// clang-format off
static const MenuItem menu_items[] = {
    SUBMENU_ITEM(0, STR_MAIN_MENU, 0, 1, 0, 0),

    SUBMENU_ITEM(1, STR_PROCESS_PAGE, 0, 100, 2, 100),
    SUBMENU_ITEM(2, STR_COMMON_SETTINGS, 0, 200, 3, 200),
    SUBMENU_ITEM(3, STR_SPECIAL_SETTINGS, 0, 300, 4, 300),
    ACTION_ITEM(4, STR_IO_MONITOR, 0, 5, open_io_monitor),
    SUBMENU_ITEM(5, STR_IO_DEBUG, 0, 400, 6, 400),
    SUBMENU_ITEM(6, STR_ALARM_LOG, 0, 500, 0, 500),

    SUBMENU_ITEM(100, STR_PROCESS_PAGE, 1, 101, 0, 100),
    VALUE_ITEM(101, STR_PART_NUMBER, 100, 102, 12906U, 0, 5, 0),
    SUBMENU_ITEM(102, STR_PART1, 100, 110, 103, 110),
    SUBMENU_ITEM(103, STR_PART2, 100, 120, 104, 120),
    SUBMENU_ITEM(104, STR_PART3, 100, 130, 105, 130),
    SUBMENU_ITEM(105, STR_PART4, 100, 140, 106, 140),
    SUBMENU_ITEM(106, STR_PART5, 100, 150, 107, 150),
    SUBMENU_ITEM(107, STR_PART6, 100, 160, 0, 160),

    SUBMENU_ITEM(110, STR_PART1, 102, 111, 0, 110),
    VALUE_ITEM(111, STR_WORKPIECE_LENGTH, 110, 112, 12414U, 10, 1000, 0),
    VALUE_ITEM(112, STR_CLAMP_OPEN_FORCE, 110, 113, 12415U, 10, 100, 0),
    VALUE_ITEM(113, STR_CLAMP_OPEN_SPEED, 110, 114, 12416U, 15, 100, 0),
    VALUE_ITEM(114, STR_CLAMP_CLOSE_FORCE, 110, 115, 12417U, 10, 100, 0),
    VALUE_ITEM(115, STR_CLAMP_CLOSE_SPEED, 110, 116, 12418U, 2, 20, 0),
    VALUE_ITEM(116, STR_OIL_PUMP_FREQUENCY, 110, 0, 12419U, 1, 5, 0),

    SUBMENU_ITEM(120, STR_PART2, 103, 121, 0, 120),
    VALUE_ITEM(121, STR_WORKPIECE_LENGTH, 120, 122, 12464U, 10, 1000, 0),
    VALUE_ITEM(122, STR_CLAMP_OPEN_FORCE, 120, 123, 12465U, 10, 100, 0),
    VALUE_ITEM(123, STR_CLAMP_OPEN_SPEED, 120, 124, 12466U, 15, 100, 0),
    VALUE_ITEM(124, STR_CLAMP_CLOSE_FORCE, 120, 125, 12467U, 10, 100, 0),
    VALUE_ITEM(125, STR_CLAMP_CLOSE_SPEED, 120, 126, 12468U, 2, 20, 0),
    VALUE_ITEM(126, STR_OIL_PUMP_FREQUENCY, 120, 0, 12469U, 1, 5, 0),

    SUBMENU_ITEM(130, STR_PART3, 104, 131, 0, 130),
    VALUE_ITEM(131, STR_WORKPIECE_LENGTH, 130, 132, 12514U, 10, 1000, 0),
    VALUE_ITEM(132, STR_CLAMP_OPEN_FORCE, 130, 133, 12515U, 10, 100, 0),
    VALUE_ITEM(133, STR_CLAMP_OPEN_SPEED, 130, 134, 12516U, 15, 100, 0),
    VALUE_ITEM(134, STR_CLAMP_CLOSE_FORCE, 130, 135, 12517U, 10, 100, 0),
    VALUE_ITEM(135, STR_CLAMP_CLOSE_SPEED, 130, 136, 12518U, 2, 20, 0),
    VALUE_ITEM(136, STR_OIL_PUMP_FREQUENCY, 130, 0, 12519U, 1, 5, 0),

    SUBMENU_ITEM(140, STR_PART4, 105, 141, 0, 140),
    VALUE_ITEM(141, STR_WORKPIECE_LENGTH, 140, 142, 12564U, 10, 1000, 0),
    VALUE_ITEM(142, STR_CLAMP_OPEN_FORCE, 140, 143, 12565U, 10, 100, 0),
    VALUE_ITEM(143, STR_CLAMP_OPEN_SPEED, 140, 144, 12566U, 15, 100, 0),
    VALUE_ITEM(144, STR_CLAMP_CLOSE_FORCE, 140, 145, 12567U, 10, 100, 0),
    VALUE_ITEM(145, STR_CLAMP_CLOSE_SPEED, 140, 146, 12568U, 2, 20, 0),
    VALUE_ITEM(146, STR_OIL_PUMP_FREQUENCY, 140, 0, 12569U, 1, 5, 0),

    SUBMENU_ITEM(150, STR_PART5, 106, 151, 0, 150),
    VALUE_ITEM(151, STR_WORKPIECE_LENGTH, 150, 152, 12614U, 10, 1000, 0),
    VALUE_ITEM(152, STR_CLAMP_OPEN_FORCE, 150, 153, 12615U, 10, 100, 0),
    VALUE_ITEM(153, STR_CLAMP_OPEN_SPEED, 150, 154, 12616U, 15, 100, 0),
    VALUE_ITEM(154, STR_CLAMP_CLOSE_FORCE, 150, 155, 12617U, 10, 100, 0),
    VALUE_ITEM(155, STR_CLAMP_CLOSE_SPEED, 150, 156, 12618U, 2, 20, 0),
    VALUE_ITEM(156, STR_OIL_PUMP_FREQUENCY, 150, 0, 12619U, 1, 5, 0),

    SUBMENU_ITEM(160, STR_PART6, 107, 161, 0, 160),
    VALUE_ITEM(161, STR_WORKPIECE_LENGTH, 160, 162, 12664U, 10, 1000, 0),
    VALUE_ITEM(162, STR_CLAMP_OPEN_FORCE, 160, 163, 12665U, 10, 100, 0),
    VALUE_ITEM(163, STR_CLAMP_OPEN_SPEED, 160, 164, 12666U, 15, 100, 0),
    VALUE_ITEM(164, STR_CLAMP_CLOSE_FORCE, 160, 165, 12667U, 10, 100, 0),
    VALUE_ITEM(165, STR_CLAMP_CLOSE_SPEED, 160, 166, 12668U, 2, 20, 0),
    VALUE_ITEM(166, STR_OIL_PUMP_FREQUENCY, 160, 0, 12669U, 1, 5, 0),

    SUBMENU_ITEM(200, STR_COMMON_SETTINGS, 2, 201, 0, 200),
    VALUE_ITEM(201, STR_SETTING_P00, 200, 202, 11916U, 10, 1000, 0),
    VALUE_ITEM(202, STR_SETTING_P01, 200, 203, 11914U, 10, 100, 0),
    VALUE_ITEM(203, STR_SETTING_P02, 200, 204, 11918U, 15, 100, 0),
    VALUE_ITEM(204, STR_SETTING_P03, 200, 205, 11915U, 10, 100, 0),
    VALUE_ITEM(205, STR_SETTING_P04, 200, 206, 12013U, 2, 50, 0),
    VALUE_ITEM(206, STR_SETTING_P05, 200, 207, 11917U, 0, 1000, 0),
    VALUE_ITEM(207, STR_SETTING_P06, 200, 208, 11920U, 0, 65535, 0),
    VALUE_ITEM(208, STR_SETTING_P07, 200, 209, 11922U, 0, 65535, 0),
    INFO_ITEM_WORD(209, STR_SETTING_P071, 200, 210, 11921U, 0),
    VALUE_ITEM(210, STR_SETTING_P08, 200, 211, 11923U, 0, 65535, 0),
    VALUE_ITEM(211, STR_SETTING_P09, 200, 212, 11924U, 0, 65535, 0),
    VALUE_ITEM(212, STR_SETTING_P10, 200, 213, 11925U, 0, 65535, 0),
    VALUE_ITEM(213, STR_SETTING_P11, 200, 214, 11926U, 0, 1000, 0),
    VALUE_ITEM(214, STR_SETTING_P12, 200, 215, 11927U, 10, 1000, 0),
    VALUE_ITEM(215, STR_SETTING_P13, 200, 216, 11928U, 10, 1000, 0),
    VALUE_ITEM(216, STR_SETTING_P14, 200, 217, 11929U, 10, 5000, 0),
    VALUE_ITEM(217, STR_SETTING_P15, 200, 218, 11930U, 5, 5000, 0),
    VALUE_ITEM(218, STR_SETTING_P16, 200, 219, 12011U, 30, 200, 0),
    VALUE_ITEM(219, STR_SETTING_P17, 200, 220, 12012U, 30, 200, 0),
    VALUE_ITEM(220, STR_SETTING_P18, 200, 0, 11934U, 200, 500, 1),

    SUBMENU_ITEM(300, STR_SPECIAL_SETTINGS, 3, 301, 0, 300),
    TOGGLE_ITEM(301, STR_M00, 300, 302, 16000U, yes_no_option_ids),
    TOGGLE_ITEM(302, STR_M01, 300, 303, 16006U, yes_no_option_ids),
    TOGGLE_ITEM(303, STR_M02, 300, 304, 16007U, yes_no_option_ids),
    TOGGLE_ITEM(304, STR_M03, 300, 305, 16008U, yes_no_option_ids),
    LIST_ITEM_BIT(305, STR_M04, 300, 306, 16009U, dao_xin_option_ids),
    TOGGLE_ITEM(306, STR_M05, 300, 307, 16010U, yes_no_option_ids),
    TOGGLE_ITEM(307, STR_M06, 300, 308, 16011U, yes_no_option_ids),
    TOGGLE_ITEM(308, STR_M07, 300, 309, 16012U, yes_no_option_ids),
    TOGGLE_ITEM(309, STR_M08, 300, 310, 16013U, yes_no_option_ids),
    TOGGLE_ITEM(310, STR_M09, 300, 311, 16014U, yes_no_option_ids),
    TOGGLE_ITEM(311, STR_M10, 300, 312, 16015U, yes_no_option_ids),
    TOGGLE_ITEM(312, STR_M11, 300, 313, 16016U, yes_no_option_ids),
    TOGGLE_ITEM(313, STR_M12, 300, 314, 16018U, yes_no_option_ids),
    TOGGLE_ITEM(314, STR_M13, 300, 315, 16019U, yes_no_option_ids),
    TOGGLE_ITEM(315, STR_M14, 300, 316, 16023U, yes_no_option_ids),
    TOGGLE_ITEM(316, STR_M15, 300, 317, 16024U, yes_no_option_ids),
    TOGGLE_ITEM(317, STR_M16, 300, 318, 16025U, yes_no_option_ids),
    TOGGLE_ITEM(318, STR_M17, 300, 319, 16026U, yes_no_option_ids),
    VALUE_ITEM(319, STR_M18, 300, 320, 12014U, 0, 99, 0),
    LIST_ITEM_BIT(320, STR_M19, 300, 321, 16028U, flash_mode_option_ids),
    LIST_ITEM_BIT(321, STR_M20, 300, 322, 16375U, language_option_ids),
    VALUE_ITEM(322, STR_SYNC_RESOLUTION, 300, 323, 12016U, 1, 9999, 0),
    VALUE_ITEM(323, STR_START_TIME, 300, 324, 12015U, 0, 100, 1),
    INFO_ITEM_DWORD(324, STR_M21, 300, 325, 12912U, 0),
    VALUE_ITEM(325, STR_TIME_YEAR, 300, 326, 12907U, 2026, 9999, 0),
    VALUE_ITEM(326, STR_TIME_MONTH, 300, 327, 12908U, 1, 12, 0),
    VALUE_ITEM(327, STR_TIME_DAY, 300, 328, 12909U, 1, 31, 0),
    VALUE_ITEM(328, STR_TIME_HOUR, 300, 329, 12910U, 0, 24, 0),
    VALUE_ITEM(329, STR_TIME_MINUTE, 300, 0, 12911U, 0, 60, 0),

    SUBMENU_ITEM(400, STR_IO_DEBUG, 5, 401, 0, 400),
    SUBMENU_ITEM(401, STR_IO_TYPE_SETTINGS, 400, 410, 402, 410),
    SUBMENU_ITEM(402, STR_IO_TEST_FUNCTION, 400, 420, 0, 420),

    SUBMENU_ITEM(410, STR_IO_TYPE_SETTINGS, 401, 411, 0, 410),
    LIST_ITEM_BIT(411, STR_LATHE_CLAMP_OPEN_TYPE, 410, 412, 16001U, normally_open_close_option_ids),
    LIST_ITEM_BIT(412, STR_LATHE_FAULT_TYPE, 410, 413, 16002U, normally_open_close_option_ids),
    LIST_ITEM_BIT(413, STR_ALLOW_BACKWARD_TYPE, 410, 414, 16003U, normally_open_close_option_ids),
    LIST_ITEM_BIT(414, STR_FORCE_PAUSE_TYPE, 410, 415, 16004U, normally_open_close_option_ids),
    LIST_ITEM_BIT(415, STR_COVER_OPEN_AT_POS_TYPE, 410, 416, 16029U, normally_open_close_option_ids),
    LIST_ITEM_BIT(416, STR_COVER_CLOSE_AT_POS_TYPE, 410, 417, 16030U, normally_open_close_option_ids),
    LIST_ITEM_BIT(417, STR_REMAIN_MATERIAL_TYPE, 410, 418, 16031U, normally_open_close_option_ids),
    LIST_ITEM_BIT(418, STR_CUT_END_SENSOR_TYPE, 410, 419, 16032U, normally_open_close_option_ids),
    LIST_ITEM_BIT(419, STR_HOME_SENSOR_TYPE, 410, 0, 16033U, normally_open_close_option_ids),

    SUBMENU_ITEM(420, STR_IO_TEST_FUNCTION, 402, 421, 0, 420),
    MOMENTARY_ITEM(421, STR_JOG_TEST, 420, 422, 17927U, on_off_option_ids),
    MOMENTARY_ITEM(422, STR_FAULT_TEST, 420, 423, 255U, on_off_option_ids),
    MOMENTARY_ITEM(423, STR_MATERIAL_SHORTAGE_TEST, 420, 424, 254U, on_off_option_ids),
    MOMENTARY_ITEM(424, STR_START_TEST, 420, 425, 17930U, on_off_option_ids),
    MOMENTARY_ITEM(425, STR_OPEN_COVER_TEST, 420, 426, 17932U, on_off_option_ids),
    MOMENTARY_ITEM(426, STR_CLOSE_COVER_TEST, 420, 427, 17933U, on_off_option_ids),
    TOGGLE_ITEM(427, STR_CLAMP_SEAT_TEST, 420, 428, 17934U, on_off_option_ids),
    TOGGLE_ITEM(428, STR_CUT_END_DISCHARGE_TEST, 420, 0, 17935U, on_off_option_ids),

    SUBMENU_ITEM(500, STR_ALARM_LOG, 6, 0, 0, 500),
};
// clang-format on

#define MENU_ITEM_COUNT (sizeof(menu_items) / sizeof(menu_items[0]))

void test_menu_navigation(void) {
    menu_system_init();
    (void)menu_configure_io_monitor(&io_monitor_config);
    (void)menu_configure_alarm_log(&alarm_log_config);
    (void)menu_load_config(menu_items, MENU_ITEM_COUNT);
    menu_start(0);
}
