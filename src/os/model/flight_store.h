/*!
    \file    src/os/model/flight_store.h
    \brief   In-Memory Flight Record Store & JSON Export Interface
*/

#ifndef AERODENS_FLIGHT_STORE_H
#define AERODENS_FLIGHT_STORE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FLIGHT_STORE_MAX_FLIGHTS    16U
#define FLIGHT_STR_LEN_SHORT        16U
#define FLIGHT_STR_LEN_MED          32U

typedef struct {
    char flight_no[FLIGHT_STR_LEN_SHORT];
    char origin[FLIGHT_STR_LEN_SHORT];
    char dest[FLIGHT_STR_LEN_SHORT];
    char aircraft_type[FLIGHT_STR_LEN_MED];
    char depart_time[FLIGHT_STR_LEN_MED];
    char arrive_time[FLIGHT_STR_LEN_MED];
    uint32_t base_zfw_kg;
    uint32_t planned_fuel_l;
    uint8_t saf_blend_pct;
    bool is_completed;
} flight_record_t;

void flight_store_init(void);
uint32_t flight_store_get_count(void);
const flight_record_t *flight_store_get(uint32_t index);
bool flight_store_add(const flight_record_t *flight);
size_t flight_store_export_json(char *buf, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif /* AERODENS_FLIGHT_STORE_H */

