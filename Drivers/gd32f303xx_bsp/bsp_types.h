/**
 * @file bsp_types.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2025-02-24
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef __BSP_TYPES_H__
#define __BSP_TYPES_H__
#include <stdint.h>

#ifndef NULL
#define NULL ((void*)0)
#endif

typedef enum _BSP_StatusTypedef{
    BSP_OK = 0,
    BSP_ERROR,
    BSP_BUSY,
    BSP_TIMEOUT,
    BSP_DEFAULT
} BSP_StatusTypedef;

/* 可变长数组结构体 */
struct u8vla {
    uint32_t len;
    uint8_t buffer[];
};

struct u16vla {
    uint32_t len;
    uint16_t buffer[];
};

struct u32vla {
    uint32_t len;
    uint32_t buffer[];
};

/* 链表结构体 */
struct list_head {
    struct list_head *next, *prev;
};

static inline void INIT_LIST_HEAD(struct list_head *list)
{
    list->next = list;
    list->prev = list;
}

static inline void __list_add(struct list_head *new, struct list_head *prev, struct list_head *next)
{
    prev->next = new;
    new->next = next;
    new->prev = prev;
    next->prev = new;
}

static inline void list_add(struct list_head *new, struct list_head *head)
{
    __list_add(new, head, head->next);
}

static inline void list_add_tail(struct list_head *new, struct list_head *head)
{
    __list_add(new, head->prev, head);
}

static inline void __list_del(struct list_head *prev, struct list_head *next)
{
    next->prev = prev;
    prev->next = next;
}

static inline void list_del(struct list_head *entry)
{
    __list_del(entry->prev, entry->next);
    entry->next = NULL;
    entry->prev = NULL;
}

#define list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

#define list_for_each_safe(pos, n, head) \
    for (pos = (head)->next, n = pos->next; pos != (head); pos = n, n = pos->next)
/* 获取宿主结构体指针 */
#define list_entry(ptr, type, member) \
    ((type *)((char *)(ptr) - (unsigned long)(&((type *)0)->member)))

/* 判断链表是否为空 */
#define list_empty(head) ((head)->next == (head))

#endif /* __BSP_TYPES_H__ */
