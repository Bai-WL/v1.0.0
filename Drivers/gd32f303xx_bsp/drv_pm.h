/**
 * @file drv_pm.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.5
 * @date 2025-08-19
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef __DRV_PM_H__
#define __DRV_PM_H__
#include <stdint.h>

void pm_handler(void);
void pm_shutdown(void);
void pm_suspend_request(void);
void pm_dont_sleep(void);
void pm_power_saver(void);
uint8_t pm_get_lpsta(void);
uint8_t pm_get_req(void);
void pm_power_on(void);

#endif /* __DRV_PM_H__ */
