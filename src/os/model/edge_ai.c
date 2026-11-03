/*!
    \file    src/os/model/edge_ai.c
    \brief   AeroDENS Edge AI Physics & Closed-Loop Advisory Engine Implementation
*/

#include "edge_ai.h"
#include <rtthread.h>
#include <math.h>
#include <string.h>

static double total_banked_fuel_kg = 0.0;
static rt_tick_t last_tick = 0;

void edge_ai_reset(void)
{
    total_banked_fuel_kg = 0.0;
    last_tick = 0;
}

void edge_ai_compute(const edge_ai_input_t *in, edge_ai_output_t *out)
{
    if (in == NULL || out == NULL) {
        return;
    }

    memset(out, 0, sizeof(*out));

    rt_tick_t current_tick = rt_tick_get();
    float delta_time_sec = (float)(current_tick - last_tick) / RT_TICK_PER_SECOND;
    last_tick = current_tick;

    if (delta_time_sec > 5.0f || delta_time_sec <= 0.0f) {
        delta_time_sec = 0.0f;
    }

    const double CO2_EMISSION_RATIO = 3.16;

    float saf_ratio = (float)in->saf_blend_pct / 100.0f;

    out->base_density = (saf_ratio * 0.759f) + ((1.0f - saf_ratio) * 0.803f);
    out->live_saf_density = out->base_density - ((in->oat_c - 15.0f) * 0.0007f);
    if (out->live_saf_density < 0.1f) {
        out->live_saf_density = 0.1f;
    }

    float total_fuel_kg = in->left_tank_kg + in->right_tank_kg;

    out->indicated_volume_l = total_fuel_kg / 0.803f;
    out->true_fuel_weight_kg = out->indicated_volume_l * out->live_saf_density;
    out->true_gross_weight_kg = in->zfw_kg + out->true_fuel_weight_kg;
    out->fms_gw_variance_kg = in->total_weight_kg - out->true_gross_weight_kg;
    out->target_refuel_volume_l = in->planned_fuel_kg / out->live_saf_density;

    float optimal_alt_ft = 39000.0f - ((out->true_gross_weight_kg - 45000.0f) * 0.2f);
    if (optimal_alt_ft > 39000.0f) {
        optimal_alt_ft = 39000.0f;
    }
    if (optimal_alt_ft < 31000.0f) {
        optimal_alt_ft = 31000.0f;
    }
    out->optimal_alt_ft = optimal_alt_ft;
    out->target_fl = (int)(optimal_alt_ft / 100.0f);

    float thermal_savings_hr = 0.0f;
    float aero_savings_hr = 0.0f;
    float active_speed_savings_hr = 0.0f;
    float altitude_deficit = optimal_alt_ft - in->alt_ft;

    if (in->alt_ft > 1000.0f) {
        if (out->fms_gw_variance_kg > 0.0f) {
            thermal_savings_hr = (out->fms_gw_variance_kg / 1000.0f) * 30.0f;
        }
        if (altitude_deficit > 0.0f && in->alt_ft > 20000.0f) {
            aero_savings_hr = (altitude_deficit / 1000.0f) * 15.0f;
        }
        if (in->mach > 0.77f && in->alt_ft > 20000.0f) {
            active_speed_savings_hr = 96.0f;
        }
    }

    float step_climb_savings_hr = thermal_savings_hr + aero_savings_hr;

    float potential_leak_hr = 0.0f;
    float achieved_savings_hr = 0.0f;

    if (in->mach > 0.77f && in->alt_ft > 20000.0f) {
        potential_leak_hr += 96.0f;
    } else if (in->mach >= 0.75f && in->mach <= 0.77f && in->alt_ft > 20000.0f) {
        achieved_savings_hr += 96.0f;
    }

    if (altitude_deficit > 500.0f && in->alt_ft > 20000.0f) {
        potential_leak_hr += (altitude_deficit / 1000.0f) * 15.0f;
    } else if (altitude_deficit <= 500.0f && in->alt_ft > 20000.0f) {
        achieved_savings_hr += 30.0f;
    }

    double savings_this_frame = (double)achieved_savings_hr * ((double)delta_time_sec / 3600.0);

    total_banked_fuel_kg += savings_this_frame;
    double total_banked_co2_kg = total_banked_fuel_kg * CO2_EMISSION_RATIO;

    out->thermal_savings_hr = thermal_savings_hr;
    out->aero_savings_hr = aero_savings_hr;
    out->step_climb_savings_hr = step_climb_savings_hr;
    out->active_speed_savings_hr = active_speed_savings_hr;
    out->potential_leak_hr = potential_leak_hr;
    out->achieved_savings_hr = achieved_savings_hr;
    out->total_banked_fuel_kg = total_banked_fuel_kg;
    out->total_banked_co2_kg = total_banked_co2_kg;

    out->trigger_refuel = (in->alt_ft < 100.0f &&
                           in->mach == 0.0f &&
                           total_fuel_kg < (in->planned_fuel_kg - 50.0f));
    out->fulfill_refuel = (total_fuel_kg >= in->planned_fuel_kg);

    out->trigger_step_climb = (step_climb_savings_hr > 0.1f && in->alt_ft > 20000.0f);
    out->fulfill_step_climb = (step_climb_savings_hr <= 0.1f || in->alt_ft <= 20000.0f);

    out->trigger_trim_speed = (in->mach > 0.77f && in->alt_ft > 20000.0f);
    out->fulfill_trim_speed = (in->mach >= 0.75f && in->mach <= 0.77f);

    float tank_imbalance = fabsf(in->left_tank_kg - in->right_tank_kg);
    out->trigger_cg_shift = (tank_imbalance > 50.0f);
    out->fulfill_cg_shift = (tank_imbalance <= 10.0f);
}
