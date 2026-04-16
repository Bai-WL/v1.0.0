#ifndef SCREEN_IDS_H
#define SCREEN_IDS_H

#include <stdint.h>

// 定义所有屏幕的唯一 ID
typedef enum {
    SCREEN_ID_INVALID = 0xFF,
    SCREEN_ID_1 = 0,
    SCREEN_ID_2,
    SCREEN_ID_3,
    SCREEN_ID_4,
    SCREEN_ID_5,
    SCREEN_ID_6,
    SCREEN_ID_7,
    SCREEN_ID_8,
    SCREEN_ID_9,
    SCREEN_ID_10,
    SCREEN_ID_MAX
} ScreenID;

#endif  // SCREEN_IDS_H