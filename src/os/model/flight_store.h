/*!
    \file    src/os/model/flight_store.h
    \brief   SD Card Flight Record Store, JSON (flight.inf) & Telemetry CSV Logger
*/

#ifndef AERODENS_FLIGHT_STORE_H
#define AERODENS_FLIGHT_STORE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "telemetry_manager.h"
#include "edge_ai.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FLIGHT_STORE_MAX_FLIGHTS    32U
#define FLIGHT_STR_LEN_SHORT        16U
#define FLIGHT_STR_LEN_MED          32U

typedef struct {
    char uuid[FLIGHT_STR_LEN_SHORT];
    char carrier[FLIGHT_STR_LEN_MED];
    char flight_no[FLIGHT_STR_LEN_SHORT];
    char origin[FLIGHT_STR_LEN_SHORT];
    char dest[FLIGHT_STR_LEN_SHORT];
    char aircraft_type[FLIGHT_STR_LEN_MED];
    char depart_time[FLIGHT_STR_LEN_MED];
    char arrive_time[FLIGHT_STR_LEN_MED];
    char timestamp_created[FLIGHT_STR_LEN_MED];
    char timestamp_last_reading[FLIGHT_STR_LEN_MED];
    uint32_t base_zfw_kg;
    uint32_t planned_fuel_l;
    uint8_t saf_blend_pct;
    bool is_completed;

    /* AeroDENS Calculated Summary Metrics */
    double fuel_saved_kg;
    double co2_reduced_kg;
    float base_density;
    float live_saf_density;
    float indicated_volume_l;
    float true_fuel_weight_kg;
    float true_gross_weight_kg;
    float fms_gw_variance_kg;
    float target_refuel_volume_l;
    float optimal_alt_ft;
    int   target_fl;
} flight_record_t;

/* Initialize SDIO, mount SD card if present, and scan /aerodens/flights */
void flight_store_init(void);

/* Poll SD card presence (detect hot-insertion and hot-removal) */
void flight_store_poll_sdcard(bool *out_state_changed, bool *out_removed_unexpectedly);

/* Check and clear pending SD card state changes detected by the background IO thread */
bool flight_store_check_events(bool *out_state_changed, bool *out_removed_unexpectedly);

/* Return true if a valid FAT32 SD card is currently mounted */
bool flight_store_is_sd_present(void);

/* Rescan /aerodens/flights/ and load all flight.inf records */
void flight_store_scan_flights(void);

/* Number of flights currently loaded from SD card */
uint32_t flight_store_get_count(void);

/* Retrieve flight record by index */
const flight_record_t *flight_store_get(uint32_t index);

/* Create a new flight folder /aerodens/flights/{uuid}, flight.inf, and flight.csv */
bool flight_store_create_flight(flight_record_t *flight);

/* Save/update flight.inf on the SD card and sync in-memory store */
bool flight_store_save_flight(const flight_record_t *flight);

/* Append one telemetry reading + Edge AI computation row to CSV and update flight record */
bool flight_store_log_telemetry(flight_record_t *flight,
                                const telemetry_data_t *raw,
                                const edge_ai_output_t *ai);

/* Export loaded flights as a JSON array string */
size_t flight_store_export_json(char *buf, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif /* AERODENS_FLIGHT_STORE_H */
