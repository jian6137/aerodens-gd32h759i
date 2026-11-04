/*!
    \file    src/os/model/telemetry_manager.h
    \brief   USART0 live telemetry receiver and packet parser
*/

#ifndef AERODENS_TELEMETRY_MANAGER_H
#define AERODENS_TELEMETRY_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool is_started;
    bool has_data;
    int32_t alt_ft;
    float mach;
    float oat_c;
    float fuel_temp_c;
    int32_t heading_deg;
    float left_tank_kg;
    float right_tank_kg;
    float total_weight_kg;
    uint32_t packet_count;
} telemetry_data_t;

void telemetry_manager_init(void);
void telemetry_manager_reset(void);
bool telemetry_manager_poll(void);
const telemetry_data_t *telemetry_manager_get(void);
bool telemetry_manager_get_snapshot(telemetry_data_t *out_data);

#ifdef __cplusplus
}
#endif

#endif /* AERODENS_TELEMETRY_MANAGER_H */
