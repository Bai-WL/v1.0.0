/**
 * @file bsp_drdusb.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.3
 * @date 2025-08-21
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef __BSP_DRDUSB_H__
#define __BSP_DRDUSB_H__
#include <stdint.h>

enum usb_role {
    DRD_HOST = 0,
    DRD_DEVICE
};

struct drdusb {
    enum usb_role role;
    uint8_t is_connect;
    uint8_t is_stay;
    uint8_t vbus_on;
    uint8_t vbus_valid;

    uint8_t scan_state;
    uint8_t stack_state;
};

extern struct drdusb hdrdusb;

#if defined(GD32F30X_CL)
void bsp_drdusb_init(void);
#elif defined(GD32F30X_HD)
void bsp_usbd_init(void);
#endif
void drdusb_scan_process(struct drdusb *hdrd);
//void drdusb_isr(usb_core_driver *udev);
void bsp_scanVbusPer10ms(void);

#endif
