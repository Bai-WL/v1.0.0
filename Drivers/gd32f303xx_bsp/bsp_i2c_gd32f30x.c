/**
 * @file bsp_i2c_gd32f30x.c
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.2
 * @date 2025-07-28
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#include "gd32f30x_gpio.h"
#include "gd32f30x_i2c.h"
#include "gd32f30x_rcu.h"
#include "gd32f30x_exti.h"
#include "bsp_types.h"
#include "bsp_i2c.h"
#include "bsp_i2c_gd32f30x.h"

static void gd32f30x_i2c_init(struct i2c_dev *dev, enum i2c_role role);
static BSP_StatusTypedef gd32f30x_i2c_write(struct i2c_dev *dev, uint8_t reg, uint8_t *data, uint32_t len);
static BSP_StatusTypedef gd32f30x_i2c_read(struct i2c_dev *dev, uint8_t reg, uint8_t *data, uint32_t len);
static void gd32f30x_i2c_isr(struct i2c_dev *dev);
static void gd32f30x_i2c_dev_rst(struct i2c_dev *dev);

/* TODO: add i2c slave handle function */
/**
 * I2C0_1: use unremaped i2c0 interface pin definition
 * I2C0_2: use remaped i2c0 interface pin definition
 */
#if (defined(GD32F30X_I2C0_1)) || (defined(GD32F30X_I2C0_2))
struct i2c_dev hi2c0 = {
    .busy = 1,      /* 锁死i2c通信通道，避免在完成初始化之前误操作 */
	.i2c_ops = {
		.init  = gd32f30x_i2c_init,
		.write = gd32f30x_i2c_write,
		.read  = gd32f30x_i2c_read,
		.isr   = gd32f30x_i2c_isr,
        .rst   = gd32f30x_i2c_dev_rst
	}
};   /* 通信控制器句柄 */
#define I2C0_SLAVE_ADDRESS7 0x64 /* 7-bit address 0110010 left shift 1-bit */
#define I2C0_SPEED 100000
#endif

static void gd32f30x_i2c_init(struct i2c_dev *dev, enum i2c_role role)
{
    rcu_periph_clock_enable(RCU_I2C0);
    rcu_periph_clock_enable(RCU_GPIOB);
#ifdef GD32F30X_I2C0_1
    gpio_init(GPIOB, GPIO_MODE_AF_OD, GPIO_OSPEED_50MHZ, GPIO_PIN_6 | GPIO_PIN_7);   // i2c0_scl | i2c0_sda
#elif defined(GD32F30X_I2C0_2)
    rcu_periph_clock_enable(RCU_AF);
    gpio_pin_remap_config(GPIO_I2C0_REMAP, ENABLE);

    gpio_init(GPIOB, GPIO_MODE_AF_OD, GPIO_OSPEED_50MHZ, GPIO_PIN_8|GPIO_PIN_9);   // i2c0_scl | i2c0_sda
#endif
    /* configure I2C clock */
    i2c_clock_config(I2C0, I2C0_SPEED, I2C_DTCY_2);
    i2c_mode_addr_config(I2C0, I2C_I2CMODE_ENABLE, I2C_ADDFORMAT_7BITS, 0x01);
    /* enable acknowledge */
    i2c_ack_config(I2C0, I2C_ACK_ENABLE);

	/* configure event interrupt of i2c */
    nvic_irq_enable(I2C0_EV_IRQn, 1, 0);
    
    i2c_interrupt_flag_clear(I2C0, I2C_INT_FLAG_SBSEND);
    i2c_interrupt_flag_clear(I2C0, I2C_INT_FLAG_ADDSEND);
    i2c_interrupt_flag_clear(I2C0, I2C_INT_FLAG_BTC);
    i2c_interrupt_flag_clear(I2C0, I2C_INT_FLAG_RBNE);
    i2c_interrupt_flag_clear(I2C0, I2C_INT_FLAG_TBE);
	i2c_interrupt_enable(I2C0, I2C_INT_EV);
	i2c_interrupt_enable(I2C0, I2C_INT_BUF);

    i2c_enable(I2C0);

    dev->role = role;
    dev->slave_addr = I2C0_SLAVE_ADDRESS7;
	dev->busy = 0;
    // dev->i2c_ops = i2c0_ops;
	// bsp_sm_init();
    // dev->sm_desc = sta_mach_register((void *)dev);
}

static BSP_StatusTypedef gd32f30x_i2c_write(struct i2c_dev *dev, uint8_t reg, uint8_t *data, uint32_t len)
{
    static enum {
        DO_I2C_SEND,
        WAIT_I2C_DONE
    } state = DO_I2C_SEND;

    switch (state) {
    case DO_I2C_SEND:
        if (!i2c_flag_get(I2C0, I2C_FLAG_I2CBSY)) {
            i2c_link_data(dev, I2C_WR, reg, data, len);
            /* send a start condition to I2C bus */
            dev->done       = 0;    /* no matter if users check done flag*/
            dev->err        = 0;    /* no matter if users check error flag*/
            dev->lsm_state  = 1;
            i2c_start_on_bus(I2C0);
            state = WAIT_I2C_DONE;
        }
        break;
    case WAIT_I2C_DONE:
        if (dev->done) {
            state = DO_I2C_SEND;
            if (dev->err)
                return BSP_ERROR;
            return BSP_OK;
        }
        break;
    default:
        break;
    }

    return BSP_BUSY;
}

static BSP_StatusTypedef gd32f30x_i2c_read(struct i2c_dev *dev, uint8_t reg, uint8_t *data, uint32_t len)
{
    static enum {
        DO_I2C_READ,
        WAIT_I2C_DONE
    } state = DO_I2C_READ;

    switch (state) {
    case DO_I2C_READ:
        if (!i2c_flag_get(I2C0, I2C_FLAG_I2CBSY)) {
            i2c_link_data(dev, I2C_RD, reg, data, len);
            /* send a start condition to I2C bus */
            dev->done       = 0;    /* no matter if users check done flag*/
            dev->err        = 0;    /* no matter if users check error flag*/
            dev->lsm_state  = 1;
            i2c_start_on_bus(I2C0);
            state = WAIT_I2C_DONE;
        }
        break;
    case WAIT_I2C_DONE:
        if (dev->done) {
            state = DO_I2C_READ;
            if (dev->err)
                return BSP_ERROR;
            return BSP_OK;
        }
        break;
    default:
        break;
    }
    
    return BSP_BUSY;
}

static void gd32f30x_i2c_isr(struct i2c_dev *dev)
{/* FIXME:对于通信状态机而言，需要获取多种不同的中断状态。因此，在此服务函数中可以一次性将中断状态寄存器的值读取出来，使用位与进行条件判断。避免多次读取寄存器产生不必要的性能浪费 */
    if (i2c_interrupt_flag_get(I2C0, I2C_INT_FLAG_AERR)) {  
        i2c_interrupt_flag_clear(I2C0, I2C_INT_FLAG_AERR);
        goto _RET;
    }

    if (dev->mode == I2C_WR) {
        switch (dev->lsm_state) {
        case 1:
            if (i2c_interrupt_flag_get(I2C0, I2C_INT_FLAG_SBSEND)) {
                i2c_master_addressing(I2C0, dev->slave_addr, I2C_TRANSMITTER);
                dev->lsm_state++;
            }
            break;
        case 2:
            if (i2c_interrupt_flag_get(I2C0, I2C_INT_FLAG_ADDSEND)) {
                i2c_interrupt_flag_clear(I2C0, I2C_INT_FLAG_ADDSEND);
                i2c_data_transmit(I2C0, dev->reg);
                dev->lsm_state++;
            }
            break;
        case 3:
            if (i2c_interrupt_flag_get(I2C0, I2C_INT_FLAG_TBE)) {
                i2c_data_transmit(I2C0, *dev->data);
                dev->ndata--;
                dev->data++;
                if (!dev->ndata) {
                    dev->lsm_state++;
                }
            }
            break;
        case 4:
            if (i2c_interrupt_flag_get(I2C0, I2C_INT_FLAG_BTC)) {
                i2c_stop_on_bus(I2C0);
                dev->lsm_state  = 0;
                dev->done       = 1;
            }
            break;
        default:
            break;
        }
    } else {
        switch (dev->lsm_state) {
        case 1:
            if (i2c_interrupt_flag_get(I2C0, I2C_INT_FLAG_SBSEND)) {
                i2c_master_addressing(I2C0, dev->slave_addr, I2C_TRANSMITTER);
                dev->lsm_state++;
            }
            break;
        case 2:
            if (i2c_interrupt_flag_get(I2C0, I2C_INT_FLAG_ADDSEND)) {
                i2c_interrupt_flag_clear(I2C0, I2C_INT_FLAG_ADDSEND);
                i2c_data_transmit(I2C0, dev->reg);
                dev->lsm_state++;
            }
            break;
        case 3:
            if (i2c_interrupt_flag_get(I2C0, I2C_INT_FLAG_BTC)) {
                i2c_start_on_bus(I2C0);
                dev->lsm_state++;
            }
            break;
        case 4:
            if (i2c_interrupt_flag_get(I2C0, I2C_INT_FLAG_SBSEND)) {
                i2c_master_addressing(I2C0, dev->slave_addr, I2C_RECEIVER);
                dev->lsm_state++;
            }
            break;
        case 5:
            if (i2c_interrupt_flag_get(I2C0, I2C_INT_FLAG_ADDSEND)) {
                i2c_interrupt_flag_clear(I2C0, I2C_INT_FLAG_ADDSEND);
				
				i2c_ackpos_config(I2C0,I2C_ACKPOS_CURRENT);
                if (dev->ndata == 1) {
                    i2c_ack_config(I2C0, I2C_ACK_DISABLE);
                    i2c_stop_on_bus(I2C0);
					dev->lsm_state += 2;
				} else if (dev->ndata == 2) {
					i2c_ackpos_config(I2C0,I2C_ACKPOS_NEXT);	/* 针对读取字节数为2的i2c通信请求需要进行特殊处理 */
					// dev->lsm_state;
				} else { 
                    i2c_ack_config(I2C0, I2C_ACK_ENABLE);
					dev->lsm_state++;
                }
				dev->lsm_state++;
            }
            break;
		case 6:
			if (i2c_interrupt_flag_get(I2C0, I2C_INT_FLAG_RBNE)) {
				i2c_ack_config(I2C0, I2C_ACK_DISABLE);
				dev->lsm_state++;
			}
			break;
        case 7:
            if (i2c_interrupt_flag_get(I2C0, I2C_INT_FLAG_BTC)) {   /* both data register and shifting register were un-empty */
                if (dev->ndata == 3) {
                    i2c_ack_config(I2C0, I2C_ACK_DISABLE);
                }
                if (dev->ndata == 2) {
					i2c_ack_config(I2C0, I2C_ACK_DISABLE);
                    i2c_stop_on_bus(I2C0);
                    dev->lsm_state++;
                }
                *dev->data = i2c_data_receive(I2C0);
                dev->data++;
                dev->ndata--;
            }
            break;
        case 8:
            if (i2c_interrupt_flag_get(I2C0, I2C_INT_FLAG_RBNE)) {   /* receive the last data byte */
                *dev->data = i2c_data_receive(I2C0);
                dev->data++;
                dev->ndata--;
                dev->lsm_state  = 0;
                dev->done       = 1;
            }
            break;
        default:
            goto _RET;  /* FIXME: 为什么需要goto _RET 20250726 */
        }
    }
	return ;
_RET:
    dev->lsm_state  = 0;
    dev->done       = 1;
    dev->err        = 1;
    i2c_stop_on_bus(I2C0);
}

void gd32f30x_i2c_dev_rst(struct i2c_dev *dev)
{
    dev->lsm_state  = 0;
    dev->done       = 1;
    dev->err        = 1;
    i2c_stop_on_bus(I2C0);
}
