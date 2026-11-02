/*!
    \file    src/os/model/telemetry_manager.c
    \brief   USART0 live telemetry receiver and packet parser
*/

#include "telemetry_manager.h"
#include "gd32h7xx.h"
#include "gd32h759i_eval.h"
#include <rtthread.h>
#include <rthw.h>
#include <stdio.h>
#include <string.h>

#define RX_RING_SIZE    512U
#define LINE_BUF_SIZE   128U

static volatile uint8_t s_rx_ring[RX_RING_SIZE];
static volatile uint16_t s_rx_head = 0U;
static volatile uint16_t s_rx_tail = 0U;

static char s_line_buf[LINE_BUF_SIZE];
static uint16_t s_line_len = 0U;

static telemetry_data_t s_telem;
static bool s_dirty = false;

void USART0_IRQHandler(void)
{
    rt_interrupt_enter();

    while (RESET != usart_flag_get(EVAL_COM0, USART_FLAG_RBNE)) {
        uint8_t ch = (uint8_t)usart_data_receive(EVAL_COM0);
        uint16_t next = (uint16_t)((s_rx_head + 1U) & (RX_RING_SIZE - 1U));
        if (next != s_rx_tail) {
            s_rx_ring[s_rx_head] = ch;
            s_rx_head = next;
        }
    }

    if (RESET != usart_flag_get(EVAL_COM0, USART_FLAG_ORERR)) {
        usart_flag_clear(EVAL_COM0, USART_FLAG_ORERR);
    }
    if (RESET != usart_flag_get(EVAL_COM0, USART_FLAG_FERR)) {
        usart_flag_clear(EVAL_COM0, USART_FLAG_FERR);
    }
    if (RESET != usart_flag_get(EVAL_COM0, USART_FLAG_NERR)) {
        usart_flag_clear(EVAL_COM0, USART_FLAG_NERR);
    }
    if (RESET != usart_flag_get(EVAL_COM0, USART_FLAG_PERR)) {
        usart_flag_clear(EVAL_COM0, USART_FLAG_PERR);
    }

    rt_interrupt_leave();
}

static void parse_line(const char *line)
{
    while (*line == ' ' || *line == '\t') {
        line++;
    }
    if (*line == '\0') {
        return;
    }

    if (strcmp(line, "START") == 0 || strstr(line, "START") != NULL) {
        s_telem.is_started = true;
        s_dirty = true;
        return;
    }

    if (strcmp(line, "STOP") == 0 || strstr(line, "STOP") != NULL) {
        s_telem.is_started = false;
        s_telem.has_data = false;
        s_dirty = true;
        return;
    }

    if (!s_telem.is_started) {
        return;
    }

    const char *pkt = strchr(line, '{');
    if (pkt != NULL) {
        int alt = 0;
        float mach = 0.0f;
        float oat = 0.0f;
        float fuel_temp = 0.0f;
        int heading = 0;
        float left_tank = 0.0f;
        float right_tank = 0.0f;
        float total_weight = 0.0f;

        if (sscanf(pkt, "{%d,%f,%f,%f,%d,%f,%f,%f}",
                   &alt,
                   &mach,
                   &oat,
                   &fuel_temp,
                   &heading,
                   &left_tank,
                   &right_tank,
                   &total_weight) == 8) {
            s_telem.alt_ft = (int32_t)alt;
            s_telem.mach = mach;
            s_telem.oat_c = oat;
            s_telem.fuel_temp_c = fuel_temp;
            s_telem.heading_deg = (int32_t)heading;
            s_telem.left_tank_kg = left_tank;
            s_telem.right_tank_kg = right_tank;
            s_telem.total_weight_kg = total_weight;
            s_telem.has_data = true;
            s_telem.packet_count++;
            s_dirty = true;
        }
    }
}

void telemetry_manager_init(void)
{
    memset(&s_telem, 0, sizeof(s_telem));
    s_rx_head = 0U;
    s_rx_tail = 0U;
    s_line_len = 0U;
    s_dirty = false;

    usart_flag_clear(EVAL_COM0, USART_FLAG_ORERR);
    usart_flag_clear(EVAL_COM0, USART_FLAG_FERR);
    usart_flag_clear(EVAL_COM0, USART_FLAG_NERR);
    usart_flag_clear(EVAL_COM0, USART_FLAG_PERR);

    nvic_irq_enable(USART0_IRQn, 2U, 0U);
    usart_interrupt_enable(EVAL_COM0, USART_INT_RBNE);
    usart_interrupt_enable(EVAL_COM0, USART_INT_ERR);
}

void telemetry_manager_reset(void)
{
    memset(&s_telem, 0, sizeof(s_telem));
    s_line_len = 0U;
    s_dirty = true;
}

bool telemetry_manager_poll(void)
{
    while (s_rx_tail != s_rx_head) {
        uint8_t ch = s_rx_ring[s_rx_tail];
        s_rx_tail = (uint16_t)((s_rx_tail + 1U) & (RX_RING_SIZE - 1U));

        if (ch == '\n' || ch == '\r') {
            if (s_line_len > 0U) {
                s_line_buf[s_line_len] = '\0';
                parse_line(s_line_buf);
                s_line_len = 0U;
            }
        } else if (ch >= 0x20U && ch <= 0x7EU) {
            if (s_line_len < (LINE_BUF_SIZE - 1U)) {
                s_line_buf[s_line_len++] = (char)ch;
            } else {
                s_line_len = 0U;
            }
        }
    }

    if (s_dirty) {
        s_dirty = false;
        return true;
    }
    return false;
}

const telemetry_data_t *telemetry_manager_get(void)
{
    return &s_telem;
}

