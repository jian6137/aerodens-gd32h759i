/*!
    \file    src/os/touch/bsp_i2c_touch.h
    \brief   Software I2C driver header for GT911 Capacitive Touch Controller on GD32H759I-EVAL
*/

#ifndef I2C_TOUCH_H
#define I2C_TOUCH_H

#include "gd32h7xx.h"

/* GT911 I2C 8-bit address (0x5D << 1 = 0xBA) */
#define GTP_ADDRESS                      0xBA

/* I2C SCL: PH7 */
#define GTP_I2C_SCL_PIN                  GPIO_PIN_7
#define GTP_I2C_SCL_GPIO_PORT            GPIOH
#define GTP_I2C_SCL_GPIO_CLK             RCU_GPIOH

/* I2C SDA: PF9 */
#define GTP_I2C_SDA_PIN                  GPIO_PIN_9
#define GTP_I2C_SDA_GPIO_PORT            GPIOF
#define GTP_I2C_SDA_GPIO_CLK             RCU_GPIOF

/* Touch RST: PF6 */
#define GTP_RST_GPIO_CLK                 RCU_GPIOF
#define GTP_RST_GPIO_PORT                GPIOF
#define GTP_RST_GPIO_PIN                 GPIO_PIN_6
#define GTP_RST_1()                      gpio_bit_set(GTP_RST_GPIO_PORT, GTP_RST_GPIO_PIN)
#define GTP_RST_0()                      gpio_bit_reset(GTP_RST_GPIO_PORT, GTP_RST_GPIO_PIN)

/* Touch INT: PH6 */
#define GTP_INT_GPIO_CLK                 RCU_GPIOH
#define GTP_INT_GPIO_PORT                GPIOH
#define GTP_INT_GPIO_PIN                 GPIO_PIN_6
#define GTP_INT_1()                      gpio_bit_set(GTP_INT_GPIO_PORT, GTP_INT_GPIO_PIN)
#define GTP_INT_0()                      gpio_bit_reset(GTP_INT_GPIO_PORT, GTP_INT_GPIO_PIN)
#define GTP_INT_GET()                    gpio_input_bit_get(GTP_INT_GPIO_PORT, GTP_INT_GPIO_PIN)

#define I2C_SCL_1()                      gpio_bit_set(GTP_I2C_SCL_GPIO_PORT, GTP_I2C_SCL_PIN)
#define I2C_SCL_0()                      gpio_bit_reset(GTP_I2C_SCL_GPIO_PORT, GTP_I2C_SCL_PIN)
#define I2C_SDA_1()                      gpio_bit_set(GTP_I2C_SDA_GPIO_PORT, GTP_I2C_SDA_PIN)
#define I2C_SDA_0()                      gpio_bit_reset(GTP_I2C_SDA_GPIO_PORT, GTP_I2C_SDA_PIN)
#define I2C_SDA_READ()                   gpio_input_bit_get(GTP_I2C_SDA_GPIO_PORT, GTP_I2C_SDA_PIN)

#define I2C_DIR_WR                       0
#define I2C_DIR_RD                       1

void i2c_resetchip(void);
void i2c_touch_init(void);
void i2c_bus_recover(void);
ErrStatus i2c_readbytes(uint8_t addr, uint8_t *buffer, uint16_t number);
ErrStatus i2c_writebytes(uint8_t addr, uint8_t *buffer, uint8_t number);
ErrStatus gt911_readreg(uint16_t addr, uint8_t *buffer, uint8_t length);
ErrStatus gt911_writereg(uint16_t addr, uint8_t *buffer, uint8_t length);

#endif /* I2C_TOUCH_H */

