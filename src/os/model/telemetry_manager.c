/*!
    \file    src/os/model/telemetry_manager.c
    \brief   USART0 live telemetry receiver and packet parser
*/

#include "telemetry_manager.h"
#include "edge_ai.h"
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

/* Double-buffering & mutex for tear-free concurrent telemetry access */
static telemetry_data_t s_telem_buf[2];
static volatile uint8_t s_active_idx = 0U;
static rt_mutex_t s_telem_mtx = RT_NULL;
static volatile bool s_dirty = false;

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
        edge_ai_reset();
        uint8_t next_idx = (uint8_t)(1U - s_active_idx);
        s_telem_buf[next_idx] = s_telem_buf[s_active_idx];
        s_telem_buf[next_idx].is_started = true;
        if (s_telem_mtx != RT_NULL) {
            rt_mutex_take(s_telem_mtx, RT_WAITING_FOREVER);
        }
        s_active_idx = next_idx;
        s_dirty = true;
        if (s_telem_mtx != RT_NULL) {
            rt_mutex_release(s_telem_mtx);
        }
        return;
    }

    if (strcmp(line, "STOP") == 0 || strstr(line, "STOP") != NULL) {
        edge_ai_reset();
        uint8_t next_idx = (uint8_t)(1U - s_active_idx);
        s_telem_buf[next_idx] = s_telem_buf[s_active_idx];
        s_telem_buf[next_idx].is_started = false;
        s_telem_buf[next_idx].has_data = false;
        if (s_telem_mtx != RT_NULL) {
            rt_mutex_take(s_telem_mtx, RT_WAITING_FOREVER);
        }
        s_active_idx = next_idx;
        s_dirty = true;
        if (s_telem_mtx != RT_NULL) {
            rt_mutex_release(s_telem_mtx);
        }
        return;
    }

    if (!s_telem_buf[s_active_idx].is_started) {
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
            uint8_t next_idx = (uint8_t)(1U - s_active_idx);
            telemetry_data_t *dst = &s_telem_buf[next_idx];
            *dst = s_telem_buf[s_active_idx];
            dst->alt_ft = (int32_t)alt;
            dst->mach = mach;
            dst->oat_c = oat;
            dst->fuel_temp_c = fuel_temp;
            dst->heading_deg = (int32_t)heading;
            dst->left_tank_kg = left_tank;
            dst->right_tank_kg = right_tank;
            dst->total_weight_kg = total_weight;
            dst->has_data = true;
            dst->packet_count++;

            if (s_telem_mtx != RT_NULL) {
                rt_mutex_take(s_telem_mtx, RT_WAITING_FOREVER);
            }
            s_active_idx = next_idx;
            s_dirty = true;
            if (s_telem_mtx != RT_NULL) {
                rt_mutex_release(s_telem_mtx);
            }
        }
    }
}

void telemetry_manager_init(void)
{
    if (s_telem_mtx == RT_NULL) {
        s_telem_mtx = rt_mutex_create("telem_mtx", RT_IPC_FLAG_PRIO);
    }
    memset(s_telem_buf, 0, sizeof(s_telem_buf));
    s_active_idx = 0U;
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
    edge_ai_reset();
    if (s_telem_mtx != RT_NULL) {
        rt_mutex_take(s_telem_mtx, RT_WAITING_FOREVER);
    }
    memset(s_telem_buf, 0, sizeof(s_telem_buf));
    s_active_idx = 0U;
    s_line_len = 0U;
    s_dirty = true;
    if (s_telem_mtx != RT_NULL) {
        rt_mutex_release(s_telem_mtx);
    }
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
    return &s_telem_buf[s_active_idx];
}

bool telemetry_manager_get_snapshot(telemetry_data_t *out_data)
{
    if (out_data == NULL) {
        return false;
    }
    if (s_telem_mtx != RT_NULL) {
        rt_mutex_take(s_telem_mtx, RT_WAITING_FOREVER);
    }
    memcpy(out_data, &s_telem_buf[s_active_idx], sizeof(telemetry_data_t));
    if (s_telem_mtx != RT_NULL) {
        rt_mutex_release(s_telem_mtx);
    }
    return out_data->has_data;
}
