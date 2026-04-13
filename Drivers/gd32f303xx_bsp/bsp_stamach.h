/**
 * @file bsp_stamach.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.1
 * @date 2025-02-24
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include "stdint.h"
#include "bsp_types.h"
#include "gd32f30x_exti.h"

typedef BSP_StatusTypedef (*SMCallbackTypeDef) (uint8_t sta);

struct sta_mach {
    void    *dev_list[32];

    uint32_t sm_list[32];
    uint32_t sm_itflag;
    uint8_t sm_sta[32];
    uint32_t sm_done;
    uint32_t busy;
    SMCallbackTypeDef sm_call[32];
    // void (*sm_done_call) (void);
};

extern struct sta_mach sm;

/**
 * @brief call software interrupt
 * 
 * @param sm_desc 
 */
static inline void sm_set_itflag(uint32_t sm_desc)
{
    sm.sm_itflag |= 1<<sm_desc;
    exti_software_interrupt_enable(EXTI_0);
}

static inline void sm_clear_itflag(uint32_t sm_desc)
{
    sm.sm_itflag &= ~(1<<sm_desc);
}

static inline uint32_t sm_get_itflag(uint32_t sm_desc)
{
    return sm.sm_itflag & (1<<sm_desc);
}

static inline void sm_set_done(uint32_t sm_desc)
{
    sm.sm_done |= 1<<sm_desc;
}

static inline void sm_clear_done(uint32_t sm_desc)
{
    sm.sm_done &= ~(1<<sm_desc);
}

static inline uint32_t sm_get_done(uint32_t sm_desc)
{
    return sm.sm_done & (1<<sm_desc);
}

static inline uint32_t sm_get_busy(uint32_t sm_desc)
{
    return sm.busy & (1<<sm_desc);
}

static inline void sm_set_busy(uint32_t sm_desc)
{
    sm.busy |= 1<<sm_desc;
}

static inline void sm_clear_busy(uint32_t sm_desc)
{
    sm.busy &= ~(1<<sm_desc);
}

static inline void sm_set_callback(uint32_t sm_desc, SMCallbackTypeDef sm_call)
{
    sm.sm_call[sm_desc] = sm_call;
}

// static inline void sm_set_done_callback(uint32_t sm_desc, void (*sm_don_call) (void))
// {
//     sm.sm_done_call[sm_desc] = sm_don_call;
// }

void bsp_sm_init(void);
uint32_t sta_mach_register(void *dev);
void sta_mach_unregister(uint32_t sm_desc);
void sta_mach_start(uint32_t sm_desc);
void swi_isr(void);
