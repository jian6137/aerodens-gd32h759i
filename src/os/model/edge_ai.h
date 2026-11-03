/*!
    \file    src/os/model/edge_ai.h
    \brief   AeroDENS Edge AI Physics & Closed-Loop Advisory Engine Interface
*/

#ifndef AERODENS_EDGE_AI_H
#define AERODENS_EDGE_AI_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t saf_blend_pct;
    float planned_fuel_kg;
    float zfw_kg;
    float alt_ft;
    float mach;
    float oat_c;
    float fuel_temp_c;
    float heading_deg;
    float left_tank_kg;
    float right_tank_kg;
    float total_weight_kg;
} edge_ai_input_t;

typedef struct {
    float base_density;
    float live_saf_density;
    float indicated_volume_l;
    float true_fuel_weight_kg;
    float true_gross_weight_kg;
    float fms_gw_variance_kg;
    float target_refuel_volume_l;

    float optimal_alt_ft;
    int target_fl;

    float thermal_savings_hr;
    float aero_savings_hr;
    float step_climb_savings_hr;
    float active_speed_savings_hr;
    float potential_leak_hr;
    float achieved_savings_hr;
    double total_banked_fuel_kg;
    double total_banked_co2_kg;

    bool trigger_refuel;
    bool fulfill_refuel;

    bool trigger_step_climb;
    bool fulfill_step_climb;

    bool trigger_trim_speed;
    bool fulfill_trim_speed;

    bool trigger_cg_shift;
    bool fulfill_cg_shift;
} edge_ai_output_t;

void edge_ai_reset(void);
void edge_ai_compute(const edge_ai_input_t *in, edge_ai_output_t *out);

#ifdef __cplusplus
}
#endif

#endif /* AERODENS_EDGE_AI_H */
