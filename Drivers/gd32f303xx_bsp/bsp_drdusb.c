/**
 * @file bsp_drdusb.c
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.4
 * @date 2025-08-21
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include <stdint.h>
//#include "drv_usb_regs.h"
//#include "drv_usb_dev.h"
#include "gd32f30x_gpio.h" 
#include "gd32f30x_rcu.h"
#include "gd32f30x_misc.h"
#include "bsp_drdusb.h"
#include "bsp_gd32f303xx_systick.h"
#include "bsp_gd32f30x_adc.h"
#include "bsp_assert.h"

struct drdusb hdrdusb;
/* means pullup */
#define usb_pullup() do {   \
			gpio_bit_reset(GPIOC, GPIO_PIN_7);  \
			gpio_bit_reset(GPIOC, GPIO_PIN_8);  \
		} while(0)
/* means pulldown */
#define usb_pulldown() do { \
			gpio_bit_set(GPIOC, GPIO_PIN_7);  \
			gpio_bit_set(GPIOC, GPIO_PIN_8);  \
		} while(0) 

#define usb_pwr_enalbe() gpio_bit_set(GPIOA, GPIO_PIN_8) 
#define usb_pwr_disable() gpio_bit_reset(GPIOA, GPIO_PIN_8)

#if defined(GD32F30X_CL)
void drd_usb_rcu_config(void)
{
    uint32_t usbfs_prescaler = 0U;
    uint32_t system_clock = rcu_clock_freq_get(CK_SYS);
    rcu_periph_clock_disable(RCU_USBFS);    /* 只有当时钟失能时，才可更改分频器 */

    if (48000000U == system_clock) {
        usbfs_prescaler = RCU_CKUSB_CKPLL_DIV1;
    } else if (72000000U  == system_clock) {
        usbfs_prescaler = RCU_CKUSB_CKPLL_DIV1_5;
    } else if (96000000U  == system_clock) {
        usbfs_prescaler = RCU_CKUSB_CKPLL_DIV2;
    } else if (120000000U == system_clock) {
        usbfs_prescaler = RCU_CKUSB_CKPLL_DIV2_5;
    } else {
        /*  reserved  */
    }

    rcu_usb_clock_config(usbfs_prescaler);
    rcu_periph_clock_enable(RCU_USBFS);
}


void bsp_drdusb_init(void)
{
    uint32_t *pul;
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(RCU_GPIOA);
    gpio_init(GPIOC, GPIO_MODE_OUT_PP, GPIO_OSPEED_10MHZ, GPIO_PIN_7|GPIO_PIN_8);   /* PC7, USB_PULLUP | PC8, USB_PULLDOWN */
    gpio_init(GPIOC, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_10MHZ, GPIO_PIN_9);         /* PC9, USB_PWR_FLAG */
    gpio_init(GPIOA, GPIO_MODE_OUT_PP, GPIO_OSPEED_10MHZ, GPIO_PIN_8);              /* PA8, USB_PWR_EN */
    usb_pwr_disable();
    usb_pulldown();

    drd_usb_rcu_config();

    pul = (uint32_t *)(0x50000000UL + 0x38UL);
    *pul |= ((1<<16) | (1<<18) | (1<<19)); /* usb_regs->gr->GCCFG |= GCCFG_PWRON | GCCFG_VBUSACEN | GCCFG_VBUSBCEN; */
}

void usb_rcu_deconfig(void)
{
    rcu_periph_clock_disable(RCU_USBFS);
}

void drd_usb_intr_config(void)
{
    nvic_irq_enable((uint8_t)USBFS_IRQn, 1U, 0U);
}

void usb_intr_deconfig(void)
{
    nvic_irq_disable((uint8_t)USBFS_IRQn);
}

void USBFS_IRQHandler(void)
{
    usbh_isr (&usbh_core);
}

#elif defined(GD32F30X_HD)
void usbd_rcu_config(void)
{
    uint32_t usbfs_prescaler = 0U;
    uint32_t system_clock = rcu_clock_freq_get(CK_SYS);
    rcu_periph_clock_disable(RCU_USBD);    /* 只有当时钟失能时，才可更改分频器 */

    if (48000000U == system_clock) {
        usbfs_prescaler = RCU_CKUSB_CKPLL_DIV1;
    } else if (72000000U  == system_clock) {
        usbfs_prescaler = RCU_CKUSB_CKPLL_DIV1_5;
    } else if (96000000U  == system_clock) {
        usbfs_prescaler = RCU_CKUSB_CKPLL_DIV2;
    } else if (120000000U == system_clock) {
        usbfs_prescaler = RCU_CKUSB_CKPLL_DIV2_5;
    } else {
        /*  reserved  */
    }

    rcu_usb_clock_config(usbfs_prescaler);
    rcu_periph_clock_enable(RCU_USBD);
}

void bsp_usbd_init(void)
{
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(RCU_GPIOA);
    gpio_init(GPIOC, GPIO_MODE_OUT_PP, GPIO_OSPEED_10MHZ, GPIO_PIN_7|GPIO_PIN_8);   /* PC7, USB_PULLUP | PC8, USB_PULLDOWN */
    gpio_init(GPIOC, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_10MHZ, GPIO_PIN_9);         /* PC9, USB_PWR_FLAG */
    gpio_init(GPIOA, GPIO_MODE_OUT_PP, GPIO_OSPEED_10MHZ, GPIO_PIN_8);              /* PA8, USB_PWR_EN */
	gpio_init(GPIOA, GPIO_MODE_IPD, GPIO_OSPEED_10MHZ, GPIO_PIN_9);
    usb_pwr_disable();
    usb_pulldown();
}

void usbd_rcu_deconfig(void)
{
    rcu_periph_clock_disable(RCU_USBD);
}

void usbd_intr_config(void)
{
    nvic_irq_enable((uint8_t)USBD_LP_CAN0_RX0_IRQn, 1U, 0U);
}

void usbd_intr_deconfig(void)
{
    nvic_irq_disable((uint8_t)USBD_LP_CAN0_RX0_IRQn);
}

void USBD_LP_CAN0_RX0_IRQHandler(void)
{
	(void)0;
}

#endif

///********************************************* cherry usb 依赖实现 *********************************************/
//void usb_dc_low_level_init(uint8_t busid)
//{
//    (void)busid;

//#if defined(GD32F30X_CL)
//    drd_usb_rcu_config();   /* 时钟初始化 */
//    drd_usb_intr_config();  /* 中断初始化 */
//#elif defined(GD32F30X_HD)
//	usbd_rcu_config();
//	usbd_intr_config();
//#endif
//    // switch (busid) {
//    // case 0: /* 从机 */
//    //     drd_usb_rcu_config();   /* 时钟初始化 */
//    //     drd_usb_intr_config();  /* 中断初始化 */
//    //     break;
//    // case 1: /* 主机 */
//    //     drd_usb_rcu_config();   /* 时钟初始化 */
//    //     drd_usb_intr_config();  /* 中断初始化 */
//    //     break;
//    // default:
//    //     break;
//    // }
//}

//void usb_dc_low_level_deinit(uint8_t busid)
//{
//    (void)busid;

//#if defined(GD32F30X_CL)
//    usbd_rcu_deconfig();
//    usbd_intr_deconfig();
//#elif defined(GD32F30X_HD)
//	usbd_rcu_deconfig();
//	usbd_intr_deconfig();
//#endif
//    
//}

//uint32_t usbd_get_dwc2_gccfg_conf(uint32_t reg_base)
//{
//    reg_base = 0;
//    reg_base |= 0<<21;  /* VBUSIG */
//    reg_base |= 0<<20;  /* SOFOEN */
//    reg_base |= 1<<19;  /* VBUSBCEN */
//    reg_base |= 1<<18;  /* VBUSACEN */
//    reg_base |= 1<<16;  /* PWRON */

//    return reg_base;
//}

//uint32_t usbh_get_dwc2_gccfg_conf(uint32_t reg_base)
//{
//    reg_base = 0;
//    reg_base |= 0<<21;  /* VBUSIG */
//    reg_base |= 0<<20;  /* SOFOEN */
//    reg_base |= 1<<19;  /* VBUSBCEN */
//    reg_base |= 1<<18;  /* VBUSACEN */
//    reg_base |= 1<<16;  /* PWRON */

//    return reg_base;
//}

//void usbd_dwc2_delay_ms(uint8_t ms)
//{
//    bsp_delay(ms);
//}
///********************************************* cherry usb 依赖实现 *********************************************/
//extern struct drdusb hdrd;
//extern void drd_device_stack_init(void);
//extern void drd_host_stack_init(void);
//extern void drd_device_stack_deinit(void);
//extern void drd_host_stack_deinit(void);
//extern void drd_device_stack_process(void);
//extern void drd_host_stack_process(void);
//void drdusb_stack_process(struct drdusb *hdrd)
//{
//    switch (hdrd->stack_state) {
//    case 0:
//        if (hdrd->is_connect) {
//            if (hdrd->role == DRD_DEVICE) {
//                drd_device_stack_init();
//            } else if (hdrd->role == DRD_HOST) {
//                drd_host_stack_init();
//            }
//        }
//        break;
//    case 1:
//        if (!(hdrd->is_connect)) {
//            if (hdrd->role == DRD_DEVICE) {
//                drd_device_stack_deinit();
//            } else if (hdrd->role == DRD_HOST) {
//                drd_host_stack_deinit();
//            }
//        }
//        if (hdrd->role == DRD_DEVICE) {
//            drd_device_stack_process();
//        } else if (hdrd->role == DRD_HOST) {
//            drd_host_stack_process();
//        }
//        break;
//    case 2:
//        break;
//    case 3:
//        break;
//    default:
//        break;
//    }
//}

//extern uint8_t CC1_STATE(void);
//extern uint8_t CC2_STATE(void);
//void VBUS_ON(void)
//{
//    gpio_bit_set(GPIOA, GPIO_PIN_8);
//}

//void VBUS_OFF(void)
//{
//    gpio_bit_reset(GPIOA, GPIO_PIN_8);
//}

//void drdusb_scan_process(struct drdusb *hdrd)
//{
//    static uint32_t last_tick = 0;
//    switch (hdrd->scan_state) {
//    case 0: /* device */
//        usb_pulldown();
//        if (hdrd->vbus_valid) {
//            hdrd->role = DRD_DEVICE;
//            hdrd->is_connect = 1;
//            hdrd->scan_state = 2;
//        }

//        if ((bsp_get_systick() - last_tick) > 50) 
//            hdrd->scan_state = 1;

//        break;
//    case 1: /* host */
//        usb_pullup();
//        if (CC1_STATE() < 0.6f || CC2_STATE() < 0.6f) {
//            VBUS_ON();
//            hdrd->vbus_on = 1;
//            hdrd->role = DRD_HOST;
//            hdrd->is_connect = 1;
//            hdrd->scan_state = 3;
//        } else if (hdrd->vbus_valid) {
//            usb_pulldown();
//            hdrd->role = DRD_DEVICE;
//            hdrd->is_connect = 1;
//            hdrd->scan_state = 2;
//        }

//        if ((bsp_get_systick() - last_tick) > 50)
//            hdrd->scan_state = 0;

//        break;
//    case 2: /* device connected */
//        if (hdrd->role == DRD_DEVICE && !hdrd->vbus_valid) {
//            hdrd->role = DRD_DEVICE;
//            hdrd->is_connect = 0;
//            hdrd->scan_state = 0;
//        } else if (hdrd->role == DRD_HOST && CC1_STATE() > 0.6f && CC2_STATE() > 0.6f) {
//            VBUS_OFF();
//            hdrd->vbus_on = 0;
//            hdrd->role = DRD_DEVICE;
//            hdrd->is_connect = 0;
//            hdrd->scan_state = 0;
//        } else {
//            (void)0;
//        }
//    default:
//        break;
//    }
//}

//void drdusb_isr(usb_core_driver *udev)
//{
//    /* SESIF: 会话中断标志位，当在B设备模式下，当检测到B设备的Vbus变为可用时，该标志位置位，即当前设备作为从设备被连接 */ 
//    /* 当从CC引脚检测到存在从设备接入时，开启供电, 并同步切换ID引脚，将USB切换为主机模式 */
//    if (HOST_MODE != (udev->regs.gr->GINTF & GINTF_COPM)) {
//        uint32_t intr = udev->regs.gr->GINTF;
//        intr &= udev->regs.gr->GINTEN;
//        /* there are no interrupts, avoid spurious interrupt */
//        if (!intr) {
//            return;
//        }
//        /* Session request interrupt */
//        if (intr & GINTF_SESIF) {
//            udev->regs.gr->GINTF = GINTF_SESIF;
//        }

//        /* OTG mode interrupt */
//        if (intr & GINTF_OTGIF) {
//            if(udev->regs.gr->GOTGINTF & GOTGINTF_SESEND) {

//            }
//            /* Clear OTG interrupt */
//            udev->regs.gr->GINTF = GINTF_OTGIF;
//        }
//    }
//}

///**
// * @brief 
// * 
// */
//void bsp_drdusb_role_detect(void)
//{
//    enum __role_detect_state {
//        DEVICE_DETECT = 0,
//        HOST_DETECT,
//        HOST_STAY,
//        HOST_CONNECTED,
//        DEVICE_CONNECTED
//    } role_detect_state = DEVICE_DETECT;
//    uint32_t last_tick = 0;
//    /**
//     * @brief 在检测的过程中，usb硬件不会被配置为主机模式，默认运行在从机模式下。只有当状态机运行到主机检测，并且检测到CC引脚电平满足要求，才会切换USB硬件，并开启USB供电。
//     * 
//     * @host_stay: 主机暂停供电，切换回从模式，但保持当前角色，不进入状态检测轮询
//     */
//    switch (role_detect_state) {
//    case DEVICE_DETECT:
//        if (hdrdusb.is_connect) {
//            last_tick = bsp_get_systick();
//            role_detect_state = HOST_CONNECTED;
//        }
//        if ((bsp_get_systick() - last_tick) > 100) {
//            /* 切换上下拉电阻，并切换检测状态 */
//            usb_pullup();
//            last_tick = bsp_get_systick();
//            role_detect_state = HOST_DETECT;
//        }
//        break;
//    case HOST_DETECT:
//        if (hdrdusb.is_connect) {
//            usb_pwr_enalbe();
//            last_tick = bsp_get_systick();
//            role_detect_state = HOST_CONNECTED;
//        }
//        if ((bsp_get_systick() - last_tick) > 100) {
//            /* 切换上下拉电阻，并切换检测状态 */
//            usb_pulldown();
//            last_tick = bsp_get_systick();
//            role_detect_state = DEVICE_DETECT;
//        }
//        break; 
//    case HOST_STAY: /* 用于低功耗，避免例如U盘忘记拔下导致的无效功耗 */
//        usb_pwr_disable();
//        if (!hdrdusb.is_connect) 
//            goto _RET_DETECT;
//        if (!hdrdusb.is_stay) {
//            usb_pwr_enalbe();
//            last_tick = bsp_get_systick();
//            role_detect_state = HOST_CONNECTED;
//        }
//        break;
//    case HOST_CONNECTED:
//        if ((bsp_get_systick() - last_tick) > 600000) { /* 10minute */
//            usb_pwr_disable();
//            hdrdusb.is_stay = 1;
//            role_detect_state = HOST_STAY;
//        }
//        if (!hdrdusb.is_connect) {
//            goto _RET_DETECT;
//        }

//        break;
//    case DEVICE_CONNECTED:

//        if (!hdrdusb.is_connect) {
//            goto _RET_DETECT;
//        }
//        break;
//    default:
//        break;
//    }
//_RET_DETECT:    /* 默认回到从机状态进行检测 */
//    usb_pwr_disable();
//    usb_pulldown();
//    hdrdusb.is_stay = 0;
//    last_tick = bsp_get_systick();
//    role_detect_state = DEVICE_DETECT;
//    return ;
//}

//static void is_usb_connected(void) 
//{
//    if (hdrdusb.role == DRD_DEVICE) {
//    }
//}

void bsp_scanVbusPer10ms(void)
{
#if defined(GD32F30X_CL)
	uint32_t *p = (uint32_t *)0x50000000UL;
    hdrdusb.vbus_valid = ((*p) & 0x0C0000) != 0;
#elif defined(GD32F30X_HD)
	hdrdusb.vbus_valid = gpio_input_bit_get(GPIOA, GPIO_PIN_9);
#endif
}
