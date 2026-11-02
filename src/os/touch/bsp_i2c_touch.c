/*!
    \file    src/os/touch/bsp_i2c_touch.c
    \brief   Software I2C driver for GT911 Capacitive Touch Controller on GD32H759I-EVAL
*/

#include "bsp_i2c_touch.h"
#include "systick.h"
#include <rtthread.h>

static void i2c_delay(void);
static void i2c_start(void);
static void i2c_stop(void);
static void i2c_sendbyte(uint8_t byte);
static uint8_t i2c_readbyte(void);
static uint8_t i2c_waitack(void);
static void i2c_ack(void);
static void i2c_nack(void);
static void i2c_gpio_config(void);

static void touch_delay_ms(uint32_t ms)
{
    if (rt_thread_self() != RT_NULL && rt_interrupt_get_nest() == 0) {
        rt_thread_mdelay((rt_int32_t)ms);
    } else {
        delay_1ms(ms);
    }
}

void i2c_resetchip(void)
{
    /* Initialize GT911: RST low, INT low -> selects I2C address 0xBA (0x5D << 1) */
    GTP_RST_0();
    GTP_INT_0();
    touch_delay_ms(2);

    /* Release RST high while INT is low (>5ms) */
    GTP_RST_1();
    touch_delay_ms(10);

    /* Configure INT as floating input */
    gpio_mode_set(GTP_INT_GPIO_PORT, GPIO_MODE_INPUT, GPIO_PUPD_NONE, GTP_INT_GPIO_PIN);
}

void i2c_touch_init(void)
{
    i2c_gpio_config();
    i2c_resetchip();
}

ErrStatus i2c_readbytes(uint8_t addr, uint8_t *buffer, uint16_t number)
{
    i2c_start();
    i2c_sendbyte(addr | I2C_DIR_RD);

    if (i2c_waitack() != 0) {
        i2c_stop();
        return ERROR;
    }

    while (number) {
        if (number == 1) {
            i2c_nack();
            i2c_stop();
        }
        *buffer = i2c_readbyte();
        buffer++;
        number--;
        if (number > 0) {
            i2c_ack();
        }
    }

    i2c_stop();
    return SUCCESS;
}

ErrStatus i2c_writebytes(uint8_t addr, uint8_t *buffer, uint8_t number)
{
    __IO uint16_t m;
    ErrStatus result = ERROR;

    i2c_stop();

    for (m = 0; m < 1000; m++) {
        i2c_start();
        i2c_sendbyte(addr | I2C_DIR_WR);
        if (i2c_waitack() == 0) {
            result = SUCCESS;
            break;
        }
    }

    if (result == SUCCESS) {
        while (number--) {
            i2c_sendbyte(*buffer);
            if (i2c_waitack() != 0) {
                result = ERROR;
                break;
            }
            buffer++;
        }
    }

    i2c_stop();
    return result;
}

void i2c_bus_recover(void)
{
    I2C_SDA_1();
    for (int k = 0; k < 9; k++) {
        I2C_SCL_0();
        i2c_delay();
        I2C_SCL_1();
        i2c_delay();
        if (I2C_SDA_READ()) {
            break;
        }
    }
    i2c_stop();
}

ErrStatus gt911_readreg(uint16_t addr, uint8_t *buffer, uint8_t length)
{
    if (length == 0U) {
        return SUCCESS;
    }

    __IO uint8_t i;

    i2c_start();
    i2c_sendbyte(GTP_ADDRESS | I2C_DIR_WR);
    if (i2c_waitack() != 0) {
        i2c_stop();
        i2c_bus_recover();
        return ERROR;
    }

    i2c_sendbyte((uint8_t)(addr >> 8));
    if (i2c_waitack() != 0) {
        i2c_stop();
        i2c_bus_recover();
        return ERROR;
    }

    i2c_sendbyte((uint8_t)(addr & 0xFFU));
    if (i2c_waitack() != 0) {
        i2c_stop();
        i2c_bus_recover();
        return ERROR;
    }

    i2c_stop();
    i2c_start();

    i2c_sendbyte(GTP_ADDRESS | I2C_DIR_RD);
    if (i2c_waitack() != 0) {
        i2c_stop();
        i2c_bus_recover();
        return ERROR;
    }

    for (i = 0; i < length - 1; i++) {
        buffer[i] = i2c_readbyte();
        i2c_ack();
    }

    buffer[i] = i2c_readbyte();
    i2c_nack();
    i2c_stop();

    return SUCCESS;
}

ErrStatus gt911_writereg(uint16_t addr, uint8_t *buffer, uint8_t length)
{
    __IO uint8_t i;

    i2c_start();
    i2c_sendbyte(GTP_ADDRESS | I2C_DIR_WR);
    if (i2c_waitack() != 0) {
        i2c_stop();
        i2c_bus_recover();
        return ERROR;
    }

    i2c_sendbyte((uint8_t)(addr >> 8));
    if (i2c_waitack() != 0) {
        i2c_stop();
        i2c_bus_recover();
        return ERROR;
    }

    i2c_sendbyte((uint8_t)(addr & 0xFFU));
    if (i2c_waitack() != 0) {
        i2c_stop();
        i2c_bus_recover();
        return ERROR;
    }

    for (i = 0; i < length; i++) {
        i2c_sendbyte(buffer[i]);
        if (i2c_waitack() != 0) {
            i2c_stop();
            i2c_bus_recover();
            return ERROR;
        }
    }

    i2c_stop();
    return SUCCESS;
}

static void i2c_delay(void)
{
    __IO uint32_t i;
    for (i = 0; i < 1200; i++) {
    }
}

static void i2c_start(void)
{
    I2C_SDA_1();
    I2C_SCL_1();
    i2c_delay();
    I2C_SDA_0();
    i2c_delay();
    I2C_SCL_0();
    i2c_delay();
}

static void i2c_stop(void)
{
    I2C_SDA_0();
    I2C_SCL_1();
    i2c_delay();
    I2C_SDA_1();
}

static void i2c_sendbyte(uint8_t byte)
{
    __IO uint32_t i;

    for (i = 0; i < 8; i++) {
        if (byte & 0x80) {
            I2C_SDA_1();
        } else {
            I2C_SDA_0();
        }
        i2c_delay();
        I2C_SCL_1();
        i2c_delay();
        I2C_SCL_0();
        if (i == 7) {
            I2C_SDA_1();
        }
        byte <<= 1;
        i2c_delay();
    }
}

static uint8_t i2c_readbyte(void)
{
    __IO uint32_t i;
    __IO uint8_t value = 0;

    for (i = 0; i < 8; i++) {
        value <<= 1;
        I2C_SCL_1();
        i2c_delay();
        if (I2C_SDA_READ()) {
            value++;
        }
        I2C_SCL_0();
        i2c_delay();
    }
    return value;
}

static uint8_t i2c_waitack(void)
{
    __IO uint8_t re;
    I2C_SDA_1();
    i2c_delay();
    I2C_SCL_1();
    i2c_delay();
    if (I2C_SDA_READ()) {
        re = 1;
    } else {
        re = 0;
    }
    I2C_SCL_0();
    i2c_delay();
    return re;
}

static void i2c_ack(void)
{
    I2C_SDA_0();
    i2c_delay();
    I2C_SCL_1();
    i2c_delay();
    I2C_SCL_0();
    i2c_delay();
    I2C_SDA_1();
}

static void i2c_nack(void)
{
    I2C_SDA_1();
    i2c_delay();
    I2C_SCL_1();
    i2c_delay();
    I2C_SCL_0();
    i2c_delay();
}

static void i2c_gpio_config(void)
{
    rcu_periph_clock_enable(GTP_I2C_SCL_GPIO_CLK);
    gpio_mode_set(GTP_I2C_SCL_GPIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, GTP_I2C_SCL_PIN);
    gpio_output_options_set(GTP_I2C_SCL_GPIO_PORT, GPIO_OTYPE_OD, GPIO_OSPEED_60MHZ, GTP_I2C_SCL_PIN);

    rcu_periph_clock_enable(GTP_I2C_SDA_GPIO_CLK);
    gpio_mode_set(GTP_I2C_SDA_GPIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, GTP_I2C_SDA_PIN);
    gpio_output_options_set(GTP_I2C_SDA_GPIO_PORT, GPIO_OTYPE_OD, GPIO_OSPEED_60MHZ, GTP_I2C_SDA_PIN);

    rcu_periph_clock_enable(GTP_RST_GPIO_CLK);
    rcu_periph_clock_enable(GTP_INT_GPIO_CLK);

    gpio_mode_set(GTP_RST_GPIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLDOWN, GTP_RST_GPIO_PIN);
    gpio_output_options_set(GTP_RST_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_12MHZ, GTP_RST_GPIO_PIN);

    gpio_mode_set(GTP_INT_GPIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLDOWN, GTP_INT_GPIO_PIN);
    gpio_output_options_set(GTP_INT_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_12MHZ, GTP_INT_GPIO_PIN);
}

