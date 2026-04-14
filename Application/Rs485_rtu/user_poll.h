#ifndef __USER_POLL_H
#define __USER_POLL_H

#include <stdint.h>
#include <string.h>
#include "user_mb_controller.h"

#ifndef SLAVE_ADDR
#define SLAVE_ADDR 1 // 默认从站地址
#endif
// 系统参数配置
#define MAX_ADDR_LIST_SIZE 64 // 最大处理地址数量
#define MAX_MERGED_BLOCKS 20  // 最大合并块数量
#define MAX_REQUESTS 30       // 最大请求队列长度
#define MAX_READ_LENGTH 48    // 单次请求最大寄存器数量(必须为偶数，否则双字装置出错)
#define MAX_MERGE_GAP 5       // 允许合并的最大地址间隔

// 地址块结构体
typedef struct
{
    uint16_t start_addr;
    uint16_t length;
} AddrBlock;

// 轮询任务类别
typedef enum
{
    POLL_ALWAYS,       // 始终读取
    POLL_CURRENT_PAGE, // 读取当前页
    POLL_TIME,
} PollType;

// 函数声明
void sort_addresses(uint16_t *arr, int size);
int merge_with_gap(uint16_t *addr_list, int addr_count, AddrBlock *blocks);
int split_blocks(AddrBlock *merged, int merged_count, AddrBlock *requests, MenuItemType type, MenuItemType *output_types);
int generate_requests(uint16_t *addr_list, MenuItemType *type_list, int addr_count, AddrBlock *output, MenuItemType *output_types);
void add_menu_polltask(void);
void add_menu_polltask_setTime(void);

// 写入值函数(供presenter调用)
#ifdef __cplusplus
extern "C"
{
#endif
    void WriteValue(uint16_t addr, uint8_t width, int32_t value);
#ifdef __cplusplus
}
#endif

#endif // __USER_POLL_H
