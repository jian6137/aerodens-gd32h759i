/*!
    \file    src/os/model/flight_store.c
    \brief   SD Card Flight Record Store, JSON (flight.inf) & Telemetry CSV Logger Implementation
*/

#include "flight_store.h"
#include "rtc_manager.h"
#include "configs/allowed_flights.h"
#include "ff.h"
#include "sdcard.h"
#include "gd32h7xx_sdio.h"
#include <rtthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define AERODENS_ROOT_DIR       "0:/aerodens"
#define AERODENS_FLIGHTS_DIR    "0:/aerodens/flights"

static FATFS s_fs;
static bool s_sd_mounted = false;
static bool s_sd_io_fault = false;
static bool s_unmountable_card_present = false;
static uint32_t s_prng_state = 0x9E3779B9UL;

/* Thread-safe event flags for non-blocking UI consumption */
static volatile bool s_sd_event_changed = false;
static volatile bool s_sd_event_removed = false;
static rt_mutex_t s_fs_mtx = RT_NULL;

static flight_record_t s_flights[FLIGHT_STORE_MAX_FLIGHTS];
static uint32_t s_flight_count = 0U;

#if FF_USE_LFN >= 1
/* CP437 upper half (0x80..0xFF) to Unicode lookup table for FatFs LFN & exFAT */
static const WCHAR s_cp437_to_uni[128] = {
    0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x00E0, 0x00E5, 0x00E7,
    0x00EA, 0x00EB, 0x00E8, 0x00EF, 0x00EE, 0x00EC, 0x00C4, 0x00C5,
    0x00C9, 0x00E6, 0x00C6, 0x00F4, 0x00F6, 0x00F2, 0x00FB, 0x00F9,
    0x00FF, 0x00D6, 0x00DC, 0x00A2, 0x00A3, 0x00A5, 0x20A7, 0x0192,
    0x00E1, 0x00ED, 0x00F3, 0x00FA, 0x00F1, 0x00D1, 0x00AA, 0x00BA,
    0x00BF, 0x2310, 0x00AC, 0x00BD, 0x00BC, 0x00A1, 0x00AB, 0x00BB,
    0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556,
    0x2555, 0x2563, 0x2551, 0x2557, 0x255D, 0x255C, 0x255B, 0x2510,
    0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x255E, 0x255F,
    0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x2567,
    0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256B,
    0x256A, 0x2518, 0x250C, 0x2588, 0x2584, 0x258C, 0x2590, 0x2580,
    0x03B1, 0x00DF, 0x0393, 0x03C0, 0x03A3, 0x03C3, 0x00B5, 0x03C4,
    0x03A6, 0x0398, 0x03A9, 0x03B4, 0x221E, 0x03C6, 0x03B5, 0x2229,
    0x2261, 0x00B1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00F7, 0x2248,
    0x00B0, 0x2219, 0x00B7, 0x221A, 0x207F, 0x00B2, 0x25A0, 0x00A0
};

WCHAR ff_oem2uni(WCHAR oem, WORD cp)
{
    (void)cp;
    if (oem < 0x80U) {
        return oem;
    }
    if (oem < 0x100U) {
        return s_cp437_to_uni[oem - 0x80U];
    }
    return 0U;
}

WCHAR ff_uni2oem(DWORD uni, WORD cp)
{
    (void)cp;
    if (uni < 0x80U) {
        return (WCHAR)uni;
    }
    for (uint32_t i = 0U; i < 128U; ++i) {
        if ((DWORD)s_cp437_to_uni[i] == uni) {
            return (WCHAR)(0x80U + i);
        }
    }
    return 0U;
}

DWORD ff_wtoupper(DWORD uni)
{
    if (uni >= 'a' && uni <= 'z') {
        return uni - 0x20U;
    }
    if (uni >= 0x00E0U && uni <= 0x00FEU && uni != 0x00F7U) {
        return uni - 0x20U;
    }
    return uni;
}
#endif

/* Override FatFs weak timestamp provider using hardware RTC */
DWORD get_fattime(void)
{
    rtc_datetime_t dt;
    rtc_manager_get_datetime(&dt);

    uint32_t yr = (dt.year >= 1980U) ? (uint32_t)(dt.year - 1980U) : 46U;
    uint32_t mo = (dt.month >= 1U && dt.month <= 12U) ? (uint32_t)dt.month : 1U;
    uint32_t dy = (dt.day >= 1U && dt.day <= 31U) ? (uint32_t)dt.day : 1U;
    uint32_t hr = (dt.hour < 24U) ? (uint32_t)dt.hour : 0U;
    uint32_t mn = (dt.minute < 60U) ? (uint32_t)dt.minute : 0U;
    uint32_t sc = (dt.second < 60U) ? (uint32_t)(dt.second / 2U) : 0U;

    return (DWORD)((yr << 25) | (mo << 21) | (dy << 16) | (hr << 11) | (mn << 5) | sc);
}

static void format_rtc_iso_timestamp(char *buf, size_t max_len)
{
    rtc_datetime_t dt;
    rtc_manager_get_datetime(&dt);
    snprintf(buf, max_len, "%04u-%02u-%02u %02u:%02u:%02u",
             (unsigned int)dt.year,
             (unsigned int)dt.month,
             (unsigned int)dt.day,
             (unsigned int)dt.hour,
             (unsigned int)dt.minute,
             (unsigned int)dt.second);
}

static uint32_t next_random_u32(void)
{
    rtc_datetime_t dt;
    rtc_manager_get_datetime(&dt);

    uint32_t entropy = (uint32_t)rt_tick_get() ^
                       (uint32_t)SysTick->VAL ^
                       ((uint32_t)dt.second << 16) ^
                       ((uint32_t)dt.minute << 8) ^
                       (uint32_t)dt.hour;

    s_prng_state ^= entropy + 0x9E3779B9UL + (s_prng_state << 6) + (s_prng_state >> 2);
    uint32_t x = s_prng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s_prng_state = x;
    return x;
}


/* Check if a currently mounted SD card is still responsive via CMD13 */
static bool sd_check_mounted_alive(void)
{
    if (s_sd_io_fault) {
        return false;
    }

    uint32_t card_status = 0U;
    if (sd_cardstatus_get(&card_status) == SD_OK) {
        return true;
    }

    /* Single immediate retry to filter transient bus noise */
    if (sd_cardstatus_get(&card_status) == SD_OK) {
        return true;
    }

    return false;
}

static void ensure_aerodens_dirs(void)
{
    FILINFO fno;
    if (f_stat(AERODENS_ROOT_DIR, &fno) != FR_OK) {
        (void)f_mkdir(AERODENS_ROOT_DIR);
    }
    if (f_stat(AERODENS_FLIGHTS_DIR, &fno) != FR_OK) {
        (void)f_mkdir(AERODENS_FLIGHTS_DIR);
    }
}

/* Lightweight JSON field extractors for flight.inf */
static bool json_extract_string(const char *json, const char *key, char *out, size_t max_len)
{
    if (json == NULL || key == NULL || out == NULL || max_len == 0U) {
        return false;
    }

    char pattern[48];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(json, pattern);
    if (p == NULL) {
        return false;
    }
    p += strlen(pattern);
    while (*p != '\0' && (*p == ' ' || *p == '\t' || *p == ':')) {
        p++;
    }
    if (*p != '"') {
        return false;
    }
    p++; /* Skip opening quote */

    size_t i = 0U;
    while (p[i] != '\0' && p[i] != '"' && p[i] != '\r' && p[i] != '\n' && (i + 1U) < max_len) {
        out[i] = p[i];
        i++;
    }
    out[i] = '\0';
    return true;
}

static bool json_extract_number(const char *json, const char *key, double *out_val)
{
    if (json == NULL || key == NULL || out_val == NULL) {
        return false;
    }

    char pattern[48];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(json, pattern);
    if (p == NULL) {
        return false;
    }
    p += strlen(pattern);
    while (*p != '\0' && (*p == ' ' || *p == '\t' || *p == ':')) {
        p++;
    }
    if (*p == '\0') {
        return false;
    }

    char *endptr = NULL;
    double v = strtod(p, &endptr);
    if (endptr == p) {
        return false;
    }
    *out_val = v;
    return true;
}

static bool parse_flight_inf(const char *json, const char *folder_uuid, flight_record_t *rec)
{
    if (json == NULL || rec == NULL) {
        return false;
    }

    memset(rec, 0, sizeof(*rec));
    if (folder_uuid != NULL) {
        strncpy(rec->uuid, folder_uuid, sizeof(rec->uuid) - 1U);
    }

    (void)json_extract_string(json, "uuid", rec->uuid, sizeof(rec->uuid));
    (void)json_extract_string(json, "carrier", rec->carrier, sizeof(rec->carrier));
    (void)json_extract_string(json, "flight_no", rec->flight_no, sizeof(rec->flight_no));
    (void)json_extract_string(json, "origin", rec->origin, sizeof(rec->origin));
    (void)json_extract_string(json, "dest", rec->dest, sizeof(rec->dest));
    (void)json_extract_string(json, "depart_time", rec->depart_time, sizeof(rec->depart_time));
    (void)json_extract_string(json, "arrive_time", rec->arrive_time, sizeof(rec->arrive_time));
    (void)json_extract_string(json, "aircraft_type", rec->aircraft_type, sizeof(rec->aircraft_type));
    (void)json_extract_string(json, "timestamp_created", rec->timestamp_created, sizeof(rec->timestamp_created));
    (void)json_extract_string(json, "timestamp_last_reading", rec->timestamp_last_reading, sizeof(rec->timestamp_last_reading));

    /* Fallback if flight_route was stored as "ORIG -> DEST" */
    if (rec->origin[0] == '\0' || rec->dest[0] == '\0') {
        char route_buf[32] = {0};
        if (json_extract_string(json, "flight_route", route_buf, sizeof(route_buf))) {
            char o[16] = {0}, d[16] = {0};
            if (sscanf(route_buf, "%15s -> %15s", o, d) == 2) {
                strncpy(rec->origin, o, sizeof(rec->origin) - 1U);
                strncpy(rec->dest, d, sizeof(rec->dest) - 1U);
            }
        }
    }

    if (rec->flight_no[0] == '\0' && rec->origin[0] == '\0') {
        return false;
    }

    if (rec->carrier[0] == '\0') {
        const airline_info_t *air = airline_get_info_by_flight_no(rec->flight_no);
        if (air != NULL && air->name != NULL) {
            strncpy(rec->carrier, air->name, sizeof(rec->carrier) - 1U);
        }
    }

    double num = 0.0;
    if (json_extract_number(json, "fuel_blend_ratio", &num)) {
        rec->saf_blend_pct = (uint8_t)num;
    }
    if (json_extract_number(json, "zero_fuel_weight_kg", &num)) {
        rec->base_zfw_kg = (uint32_t)num;
    }
    if (json_extract_number(json, "planned_fuel_l", &num)) {
        rec->planned_fuel_l = (uint32_t)num;
    }
    if (json_extract_number(json, "fuel_saved_kg", &num)) {
        rec->fuel_saved_kg = num;
    }
    if (json_extract_number(json, "co2_reduced_kg", &num)) {
        rec->co2_reduced_kg = num;
    }
    if (json_extract_number(json, "base_density", &num)) {
        rec->base_density = (float)num;
    }
    if (json_extract_number(json, "live_saf_density", &num)) {
        rec->live_saf_density = (float)num;
    }
    if (json_extract_number(json, "indicated_volume_l", &num)) {
        rec->indicated_volume_l = (float)num;
    }
    if (json_extract_number(json, "true_fuel_weight_kg", &num)) {
        rec->true_fuel_weight_kg = (float)num;
    }
    if (json_extract_number(json, "true_gross_weight_kg", &num)) {
        rec->true_gross_weight_kg = (float)num;
    }
    if (json_extract_number(json, "fms_gw_variance_kg", &num)) {
        rec->fms_gw_variance_kg = (float)num;
    }
    if (json_extract_number(json, "target_refuel_volume_l", &num)) {
        rec->target_refuel_volume_l = (float)num;
    }
    if (json_extract_number(json, "optimal_alt_ft", &num)) {
        rec->optimal_alt_ft = (float)num;
    }
    if (json_extract_number(json, "target_fl", &num)) {
        rec->target_fl = (int)num;
    }

    rec->is_completed = true;
    return true;
}

static bool write_flight_inf_file(const flight_record_t *f)
{
    if (!s_sd_mounted || f == NULL || f->uuid[0] == '\0') {
        return false;
    }

    char path[64];
    snprintf(path, sizeof(path), "%s/%s/flight.inf", AERODENS_FLIGHTS_DIR, f->uuid);

    const char *carrier_str = f->carrier;
    if (carrier_str[0] == '\0') {
        const airline_info_t *air = airline_get_info_by_flight_no(f->flight_no);
        carrier_str = (air != NULL && air->name != NULL) ? air->name : "Aviation";
    }

    char json_buf[1024];
    int len = snprintf(
        json_buf, sizeof(json_buf),
        "{\n"
        "  \"uuid\": \"%s\",\n"
        "  \"carrier\": \"%s\",\n"
        "  \"flight_no\": \"%s\",\n"
        "  \"fuel_blend_ratio\": %u,\n"
        "  \"flight_route\": \"%s -> %s\",\n"
        "  \"origin\": \"%s\",\n"
        "  \"dest\": \"%s\",\n"
        "  \"depart_time\": \"%s\",\n"
        "  \"arrive_time\": \"%s\",\n"
        "  \"aircraft_type\": \"%s\",\n"
        "  \"zero_fuel_weight_kg\": %lu,\n"
        "  \"planned_fuel_l\": %lu,\n"
        "  \"timestamp_created\": \"%s\",\n"
        "  \"timestamp_last_reading\": \"%s\",\n"
        "  \"fuel_saved_kg\": %.4f,\n"
        "  \"co2_reduced_kg\": %.4f,\n"
        "  \"base_density\": %.4f,\n"
        "  \"live_saf_density\": %.4f,\n"
        "  \"indicated_volume_l\": %.1f,\n"
        "  \"true_fuel_weight_kg\": %.1f,\n"
        "  \"true_gross_weight_kg\": %.1f,\n"
        "  \"fms_gw_variance_kg\": %.1f,\n"
        "  \"target_refuel_volume_l\": %.1f,\n"
        "  \"optimal_alt_ft\": %.0f,\n"
        "  \"target_fl\": %d\n"
        "}\n",
        f->uuid,
        carrier_str,
        f->flight_no,
        (unsigned int)f->saf_blend_pct,
        f->origin,
        f->dest,
        f->origin,
        f->dest,
        f->depart_time,
        f->arrive_time,
        f->aircraft_type,
        (unsigned long)f->base_zfw_kg,
        (unsigned long)f->planned_fuel_l,
        (f->timestamp_created[0] != '\0') ? f->timestamp_created : "N/A",
        (f->timestamp_last_reading[0] != '\0') ? f->timestamp_last_reading : "N/A",
        f->fuel_saved_kg,
        f->co2_reduced_kg,
        (double)f->base_density,
        (double)f->live_saf_density,
        (double)f->indicated_volume_l,
        (double)f->true_fuel_weight_kg,
        (double)f->true_gross_weight_kg,
        (double)f->fms_gw_variance_kg,
        (double)f->target_refuel_volume_l,
        (double)f->optimal_alt_ft,
        f->target_fl
    );

    if (len <= 0) {
        return false;
    }

    FIL fil;
    FRESULT res = f_open(&fil, path, FA_WRITE | FA_CREATE_ALWAYS);
    if (res != FR_OK) {
        if (res == FR_DISK_ERR || res == FR_NOT_READY) {
            s_sd_io_fault = true;
        }
        return false;
    }

    UINT bw = 0U;
    res = f_write(&fil, json_buf, (UINT)len, &bw);
    if (res != FR_OK || bw != (UINT)len) {
        s_sd_io_fault = true;
        (void)f_close(&fil);
        return false;
    }

    res = f_close(&fil);
    if (res != FR_OK) {
        s_sd_io_fault = true;
        return false;
    }

    return true;
}

static void sync_in_memory_flight(const flight_record_t *flight)
{
    if (flight == NULL) {
        return;
    }

    for (uint32_t i = 0U; i < s_flight_count; ++i) {
        if (strcmp(s_flights[i].uuid, flight->uuid) == 0) {
            memcpy(&s_flights[i], flight, sizeof(flight_record_t));
            return;
        }
    }

    if (s_flight_count < FLIGHT_STORE_MAX_FLIGHTS) {
        memcpy(&s_flights[s_flight_count], flight, sizeof(flight_record_t));
        s_flight_count++;
    }
}

void flight_store_scan_flights(void)
{
    s_flight_count = 0U;
    if (!s_sd_mounted) {
        return;
    }

    ensure_aerodens_dirs();

    DIR dir;
    FILINFO fno;
    if (f_opendir(&dir, AERODENS_FLIGHTS_DIR) != FR_OK) {
        return;
    }

    while (s_flight_count < FLIGHT_STORE_MAX_FLIGHTS) {
        FRESULT res = f_readdir(&dir, &fno);
        if (res != FR_OK || fno.fname[0] == '\0') {
            break;
        }
        if ((fno.fattrib & AM_DIR) == 0U) {
            continue;
        }
        if (fno.fname[0] == '.') {
            continue;
        }

        char inf_path[64];
        snprintf(inf_path, sizeof(inf_path), "%s/%s/flight.inf", AERODENS_FLIGHTS_DIR, fno.fname);

        FIL fil;
        if (f_open(&fil, inf_path, FA_READ) == FR_OK) {
            char json_buf[1024];
            UINT br = 0U;
            FRESULT rres = f_read(&fil, json_buf, sizeof(json_buf) - 1U, &br);
            (void)f_close(&fil);

            if (rres == FR_OK && br > 0U) {
                json_buf[br] = '\0';
                flight_record_t rec;
                if (parse_flight_inf(json_buf, fno.fname, &rec)) {
                    memcpy(&s_flights[s_flight_count], &rec, sizeof(flight_record_t));
                    s_flight_count++;
                }
            }
        }
    }

    (void)f_closedir(&dir);
}

void flight_store_init(void)
{
    if (s_fs_mtx == RT_NULL) {
        s_fs_mtx = rt_mutex_create("fs_mtx", RT_IPC_FLAG_PRIO);
    }

    s_flight_count = 0U;
    s_sd_mounted = false;
    s_sd_io_fault = false;
    s_unmountable_card_present = false;
    s_sd_event_changed = false;
    s_sd_event_removed = false;

    /* Direct mount attempt on drive "0:" at boot */
    FRESULT mres = f_mount(&s_fs, "0:", 1);
    if (mres == FR_OK) {
        s_sd_mounted = true;
        ensure_aerodens_dirs();
        flight_store_scan_flights();
        rt_kprintf("[SD] Mounted SD card at startup (fs_type=%u, flights=%lu)\r\n",
                   (unsigned int)s_fs.fs_type, (unsigned long)s_flight_count);
        return;
    }

    /* Clear mount if startup attempt failed */
    (void)f_mount(NULL, "0:", 0);
    rt_kprintf("[SD] Startup mount returned FRESULT=%d (no card or not ready yet)\r\n", (int)mres);
}

void flight_store_poll_sdcard(bool *out_state_changed, bool *out_removed_unexpectedly)
{
    static uint32_t s_poll_counter = 0U;

    if (out_state_changed != NULL) {
        *out_state_changed = false;
    }
    if (out_removed_unexpectedly != NULL) {
        *out_removed_unexpectedly = false;
    }

    if (s_sd_mounted) {
        if (!sd_check_mounted_alive()) {
            (void)f_mount(NULL, "0:", 0);
            s_sd_mounted = false;
            s_sd_io_fault = false;
            s_unmountable_card_present = false;
            s_flight_count = 0U;
            rt_kprintf("[SD] SD card removed or lost communication\r\n");

            if (s_fs_mtx != RT_NULL) {
                rt_mutex_take(s_fs_mtx, RT_WAITING_FOREVER);
            }
            s_sd_event_changed = true;
            s_sd_event_removed = true;
            if (s_fs_mtx != RT_NULL) {
                rt_mutex_release(s_fs_mtx);
            }

            if (out_state_changed != NULL) {
                *out_state_changed = true;
            }
            if (out_removed_unexpectedly != NULL) {
                *out_removed_unexpectedly = true;
            }
        }
    } else {
        /* Poll mount every 1.0s (4 ticks at 250ms ui_Check_Task) */
        s_poll_counter++;
        if ((s_poll_counter % 4U) == 0U) {
            FRESULT mres = f_mount(&s_fs, "0:", 1);
            if (mres == FR_OK) {
                s_sd_mounted = true;
                s_sd_io_fault = false;
                s_unmountable_card_present = false;
                ensure_aerodens_dirs();
                flight_store_scan_flights();
                rt_kprintf("[SD] SD card mounted! (fs_type=%u, flights=%lu)\r\n",
                           (unsigned int)s_fs.fs_type, (unsigned long)s_flight_count);

                if (s_fs_mtx != RT_NULL) {
                    rt_mutex_take(s_fs_mtx, RT_WAITING_FOREVER);
                }
                s_sd_event_changed = true;
                if (s_fs_mtx != RT_NULL) {
                    rt_mutex_release(s_fs_mtx);
                }

                if (out_state_changed != NULL) {
                    *out_state_changed = true;
                }
            } else {
                (void)f_mount(NULL, "0:", 0);
                if (mres == FR_NO_FILESYSTEM) {
                    rt_kprintf("[SD] Card detected but no FAT/exFAT filesystem (FRESULT=13)\r\n");
                }
            }
        }
    }
}

bool flight_store_check_events(bool *out_state_changed, bool *out_removed_unexpectedly)
{
    bool had_event = false;
    bool chg = false;
    bool rem = false;

    if (s_fs_mtx != RT_NULL) {
        rt_mutex_take(s_fs_mtx, RT_WAITING_FOREVER);
    }
    if (s_sd_event_changed || s_sd_event_removed) {
        had_event = true;
        chg = s_sd_event_changed;
        rem = s_sd_event_removed;
        s_sd_event_changed = false;
        s_sd_event_removed = false;
    }
    if (s_fs_mtx != RT_NULL) {
        rt_mutex_release(s_fs_mtx);
    }

    if (out_state_changed != NULL) {
        *out_state_changed = chg;
    }
    if (out_removed_unexpectedly != NULL) {
        *out_removed_unexpectedly = rem;
    }

    return had_event;
}

bool flight_store_is_sd_present(void)
{
    return s_sd_mounted;
}

uint32_t flight_store_get_count(void)
{
    return s_sd_mounted ? s_flight_count : 0U;
}

const flight_record_t *flight_store_get(uint32_t index)
{
    if (!s_sd_mounted || index >= s_flight_count) {
        return NULL;
    }
    return &s_flights[index];
}

bool flight_store_create_flight(flight_record_t *flight)
{
    if (!s_sd_mounted || flight == NULL) {
        return false;
    }

    ensure_aerodens_dirs();

    /* Generate unique 8-character random hex UUID folder name (valid SFN 8.3 & LFN) */
    char folder_path[64];
    FILINFO fno;
    for (uint32_t attempt = 0U; attempt < 32U; ++attempt) {
        uint32_t r = next_random_u32();
        snprintf(flight->uuid, sizeof(flight->uuid), "%08lX", (unsigned long)r);
        snprintf(folder_path, sizeof(folder_path), "%s/%s", AERODENS_FLIGHTS_DIR, flight->uuid);
        if (f_stat(folder_path, &fno) != FR_OK) {
            break;
        }
    }

    FRESULT mres = f_mkdir(folder_path);
    if (mres != FR_OK && mres != FR_EXIST) {
        if (mres == FR_DISK_ERR || mres == FR_NOT_READY) {
            s_sd_io_fault = true;
        }
        return false;
    }

    if (flight->carrier[0] == '\0') {
        const airline_info_t *air = airline_get_info_by_flight_no(flight->flight_no);
        if (air != NULL && air->name != NULL) {
            strncpy(flight->carrier, air->name, sizeof(flight->carrier) - 1U);
        }
    }

    format_rtc_iso_timestamp(flight->timestamp_created, sizeof(flight->timestamp_created));
    strncpy(flight->timestamp_last_reading, "N/A", sizeof(flight->timestamp_last_reading) - 1U);

    /* Initialize AeroDENS calculated metrics to 0 at creation */
    flight->fuel_saved_kg = 0.0;
    flight->co2_reduced_kg = 0.0;
    flight->base_density = 0.0f;
    flight->live_saf_density = 0.0f;
    flight->indicated_volume_l = 0.0f;
    flight->true_fuel_weight_kg = 0.0f;
    flight->true_gross_weight_kg = 0.0f;
    flight->fms_gw_variance_kg = 0.0f;
    flight->target_refuel_volume_l = 0.0f;
    flight->optimal_alt_ft = 0.0f;
    flight->target_fl = 0;
    flight->is_completed = false;

    if (!write_flight_inf_file(flight)) {
        return false;
    }

    /* Create telemetry CSV file with header row */
    char csv_path[64];
    snprintf(csv_path, sizeof(csv_path), "%s/%s/flight.csv", AERODENS_FLIGHTS_DIR, flight->uuid);

    FIL csv_fil;
    if (f_open(&csv_fil, csv_path, FA_WRITE | FA_CREATE_ALWAYS) == FR_OK) {
        static const char k_csv_header[] =
            "timestamp,alt_ft,mach,oat_c,fuel_temp_c,heading_deg,left_tank_kg,right_tank_kg,"
            "total_weight_kg,base_density,live_saf_density,indicated_volume_l,true_fuel_weight_kg,"
            "true_gross_weight_kg,fms_gw_variance_kg,target_refuel_volume_l,optimal_alt_ft,"
            "target_fl,step_climb_savings_hr,fuel_saved_kg,co2_reduced_kg\n";
        UINT bw = 0U;
        (void)f_write(&csv_fil, k_csv_header, (UINT)(sizeof(k_csv_header) - 1U), &bw);
        (void)f_close(&csv_fil);
    } else {
        s_sd_io_fault = true;
        return false;
    }

    sync_in_memory_flight(flight);
    return true;
}

bool flight_store_save_flight(const flight_record_t *flight)
{
    if (!s_sd_mounted || flight == NULL || flight->uuid[0] == '\0') {
        return false;
    }

    bool ok = write_flight_inf_file(flight);
    if (ok) {
        sync_in_memory_flight(flight);
    }
    return ok;
}

bool flight_store_log_telemetry(flight_record_t *flight,
                                const telemetry_data_t *raw,
                                const edge_ai_output_t *ai)
{
    if (!s_sd_mounted || flight == NULL || raw == NULL || ai == NULL || flight->uuid[0] == '\0') {
        return false;
    }

    format_rtc_iso_timestamp(flight->timestamp_last_reading, sizeof(flight->timestamp_last_reading));

    /* Update flight summary metrics */
    flight->fuel_saved_kg = ai->total_banked_fuel_kg;
    flight->co2_reduced_kg = ai->total_banked_co2_kg;
    flight->base_density = ai->base_density;
    flight->live_saf_density = ai->live_saf_density;
    flight->indicated_volume_l = ai->indicated_volume_l;
    flight->true_fuel_weight_kg = ai->true_fuel_weight_kg;
    flight->true_gross_weight_kg = ai->true_gross_weight_kg;
    flight->fms_gw_variance_kg = ai->fms_gw_variance_kg;
    flight->target_refuel_volume_l = ai->target_refuel_volume_l;
    flight->optimal_alt_ft = ai->optimal_alt_ft;
    flight->target_fl = ai->target_fl;

    char csv_path[64];
    snprintf(csv_path, sizeof(csv_path), "%s/%s/flight.csv", AERODENS_FLIGHTS_DIR, flight->uuid);

    FIL fil;
    FRESULT res = f_open(&fil, csv_path, FA_WRITE | FA_OPEN_ALWAYS);
    if (res != FR_OK) {
        s_sd_io_fault = true;
        return false;
    }

    /* Seek to end of CSV file to append row */
    res = f_lseek(&fil, f_size(&fil));
    if (res != FR_OK) {
        s_sd_io_fault = true;
        (void)f_close(&fil);
        return false;
    }

    char row_buf[320];
    int len = snprintf(
        row_buf, sizeof(row_buf),
        "%s,%ld,%.2f,%.1f,%.1f,%ld,%.1f,%.1f,%.1f,%.4f,%.4f,%.1f,%.1f,%.1f,%.1f,%.1f,%.0f,%d,%.1f,%.4f,%.4f\n",
        flight->timestamp_last_reading,
        (long)raw->alt_ft,
        (double)raw->mach,
        (double)raw->oat_c,
        (double)raw->fuel_temp_c,
        (long)raw->heading_deg,
        (double)raw->left_tank_kg,
        (double)raw->right_tank_kg,
        (double)raw->total_weight_kg,
        (double)ai->base_density,
        (double)ai->live_saf_density,
        (double)ai->indicated_volume_l,
        (double)ai->true_fuel_weight_kg,
        (double)ai->true_gross_weight_kg,
        (double)ai->fms_gw_variance_kg,
        (double)ai->target_refuel_volume_l,
        (double)ai->optimal_alt_ft,
        ai->target_fl,
        (double)ai->step_climb_savings_hr,
        ai->total_banked_fuel_kg,
        ai->total_banked_co2_kg
    );

    if (len > 0) {
        UINT bw = 0U;
        res = f_write(&fil, row_buf, (UINT)len, &bw);
        if (res != FR_OK || bw != (UINT)len) {
            s_sd_io_fault = true;
            (void)f_close(&fil);
            return false;
        }
    }

    if (f_close(&fil) != FR_OK) {
        s_sd_io_fault = true;
        return false;
    }

    /* Also keep flight.inf updated on SD card */
    if (!write_flight_inf_file(flight)) {
        return false;
    }

    sync_in_memory_flight(flight);
    return true;
}

size_t flight_store_export_json(char *buf, size_t max_len)
{
    if (buf == NULL || max_len == 0U) {
        return 0U;
    }

    size_t written = 0U;
    int n = snprintf(buf, max_len, "{\"flights\":[");
    if (n < 0 || (size_t)n >= max_len) {
        return 0U;
    }
    written += (size_t)n;

    for (uint32_t i = 0U; i < s_flight_count; ++i) {
        const flight_record_t *f = &s_flights[i];
        n = snprintf(buf + written, max_len - written,
                     "%s{\"uuid\":\"%s\",\"flight_no\":\"%s\",\"origin\":\"%s\",\"dest\":\"%s\","
                     "\"aircraft\":\"%s\",\"depart\":\"%s\",\"arrive\":\"%s\","
                     "\"zfw_kg\":%lu,\"fuel_l\":%lu,\"saf_pct\":%u}",
                     (i > 0U) ? "," : "",
                     f->uuid, f->flight_no, f->origin, f->dest,
                     f->aircraft_type, f->depart_time, f->arrive_time,
                     (unsigned long)f->base_zfw_kg,
                     (unsigned long)f->planned_fuel_l,
                     (unsigned int)f->saf_blend_pct);

        if (n < 0 || (written + (size_t)n) >= max_len) {
            break;
        }
        written += (size_t)n;
    }

    n = snprintf(buf + written, max_len - written, "]}");
    if (n > 0 && (written + (size_t)n) < max_len) {
        written += (size_t)n;
    }

    return written;
}
