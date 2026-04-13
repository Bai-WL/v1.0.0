/**
 * @file bsp_i2c.h
 * @author your name (you@domain.com)
 * @brief 
 * @version 0.2
 * @date 2025-07-28
 * 
 * @copyright Copyright (c) 2025
 * 
 */
#ifndef __BSP_I2C_H__
#define __BSP_I2C_H__
#include "stdint.h"
#include "bsp_types.h"

struct i2c_operations;
struct i2c_dev;

enum i2c_role {
    I2C_MASTER = 0,
    I2C_SLAVE
};

enum i2c_rd {
    I2C_WR = 0,
    I2C_RD
};

struct i2c_operations {
    void (*init)  (struct i2c_dev *dev, enum i2c_role role);
    BSP_StatusTypedef (*write) (struct i2c_dev *dev, uint8_t reg, uint8_t *data, uint32_t len);
    BSP_StatusTypedef (*read)  (struct i2c_dev *dev, uint8_t reg, uint8_t *data, uint32_t len);
    void (*isr)   (struct i2c_dev *dev);
    void (*rst) (struct i2c_dev *dev);
};

/**
 * struct i2c_dev for private argument
 * 
 * @role:   i2c communication role
 * @mode:   i2c write or read communication (I2C_WR I2C_RD)
 * @slave_addr: slave address
 * @pdata:  buffer of data to send or read
 * @ndata:  count of data to send or read
 * 
 */
struct i2c_dev {
    uint32_t sm_desc;
    
    /* 控制器参数 */
    uint16_t        slave_addr;
    enum i2c_role   role;
    enum i2c_rd     mode;
    /* 通信状态 */
    uint8_t         lsm_state;  /* i2c控制器发送状态机状态变量 */
    uint8_t         busy;   /* 用户可读可写 */
    uint8_t         done;   /* 用户只读 */
    uint8_t         err;    /* 用户只读 */

    /* 传输参数 */
    uint8_t         reg;    /* XXX: will be optimized to carry on i2c communication of multi-byte address. */
    uint8_t         *data;
    uint8_t         ndata;

    /* 操作函数 */
    struct i2c_operations i2c_ops;
};

typedef void (*i2c_process) (struct i2c_dev *dev, uint32_t steps);

static inline void i2c_link_data(struct i2c_dev *dev, enum i2c_rd mode, uint8_t reg, uint8_t *data, uint32_t len)
{
    dev->mode = mode;
    dev->reg  = reg;
    dev->data = data;
    dev->ndata = len;
}

static inline void i2c_rst_state(struct i2c_dev *dev)
{
    dev->lsm_state = 0;
}

#endif /* __BSP_I2C_H__ */
