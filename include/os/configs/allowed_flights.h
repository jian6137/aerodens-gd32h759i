/*!
    \file    include/configs/allowed_flights.h
    \brief   AeroDENS EFB Allowed Airlines, Route Schedules & Aircraft Types
*/

#ifndef AERODENS_ALLOWED_FLIGHTS_H
#define AERODENS_ALLOWED_FLIGHTS_H

#include <stdint.h>
#include <string.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Airline Icon Image Descriptors */
extern const lv_img_dsc_t img_ic_airline_5j;
extern const lv_img_dsc_t img_ic_airline_5j_lg;
extern const lv_img_dsc_t img_ic_airline_pr;
extern const lv_img_dsc_t img_ic_airline_pr_lg;
extern const lv_img_dsc_t img_ic_airline_tg;
extern const lv_img_dsc_t img_ic_airline_tg_lg;
extern const lv_img_dsc_t img_ic_flight_separator;

typedef enum {
    AIRLINE_5J = 0,
    AIRLINE_PR = 1,
    AIRLINE_TG = 2,
    AIRLINE_COUNT = 3
} airline_id_t;

typedef struct {
    const char *code;
    const char *name;
    const char *callsign;
    const lv_img_dsc_t *icon;
    const lv_img_dsc_t *icon_lg;
} airline_info_t;

static const airline_info_t k_allowed_airlines[AIRLINE_COUNT] = {
    { "5J", "Cebu Pacific",         "CEBU AIR",   &img_ic_airline_5j, &img_ic_airline_5j_lg },
    { "PR", "Philippine Airlines",  "PHILIPPINE", &img_ic_airline_pr, &img_ic_airline_pr_lg },
    { "TG", "ThaiAirways",          "THAI",       &img_ic_airline_tg, &img_ic_airline_tg_lg }
};

static inline const airline_info_t *airline_get_info_by_code(const char *code)
{
    if (code != NULL) {
        for (uint32_t i = 0U; i < AIRLINE_COUNT; ++i) {
            if (strncmp(code, k_allowed_airlines[i].code, 2) == 0) {
                return &k_allowed_airlines[i];
            }
        }
    }
    return &k_allowed_airlines[AIRLINE_5J];
}

static inline const airline_info_t *airline_get_info_by_flight_no(const char *flight_no)
{
    return airline_get_info_by_code(flight_no);
}

typedef struct {
    const char *code;
    const char *full_name;
    uint32_t default_zfw_kg;
    uint32_t max_fuel_l;
} aircraft_type_info_t;

static const aircraft_type_info_t k_aircraft_types[] = {
    { "A320N",    "Airbus A320neo",  41500U, 24210U },
    { "A320-200", "Airbus A320-200", 42600U, 24210U },
    { "A321N",    "Airbus A321neo",  46300U, 32940U },
    { "A330N",    "Airbus A330neo",  137000U, 139090U },
    { "A330-300", "Airbus A330-300", 129000U, 97530U }
};

#define AIRCRAFT_TYPE_COUNT (sizeof(k_aircraft_types) / sizeof(k_aircraft_types[0]))

typedef struct {
    airline_id_t airline;
    const char *flight_no;
    const char *origin;
    const char *dest;
    const char *depart_time;
    const char *arrive_time;
    const char *default_ac;
    uint32_t planned_fuel_l;
    uint32_t base_zfw_kg;
} scheduled_flight_t;

static const scheduled_flight_t k_scheduled_flights[] = {
    /* Cebu Pacific */
    { AIRLINE_5J, "5J 392", "CGY", "MNL", "8:00 PM", "9:45 PM",  "A320N", 6800U, 62400U },
    { AIRLINE_5J, "5J 390", "CGY", "MNL", "9:30 PM", "11:15 PM", "A320N", 6850U, 63100U },
    { AIRLINE_5J, "5J 380", "CGY", "MNL", "10:00 PM", "11:45 PM", "A320N", 6780U, 61900U },
    { AIRLINE_5J, "5J 383", "MNL", "CGY", "4:10 AM", "5:40 AM",  "A320N", 6900U, 64600U },
    { AIRLINE_5J, "5J 381", "MNL", "CGY", "4:57 AM", "6:25 AM",  "A320N", 6820U, 63800U },
    { AIRLINE_5J, "5J 395", "MNL", "CGY", "7:38 AM", "9:14 AM",  "A320N", 6950U, 65200U },

    /* Philippine Airlines */
    /* CGY -> MNL */
    { AIRLINE_PR, "PR 2522", "CGY", "MNL", "8:40 AM",  "10:25 AM", "A321N",    7150U, 65800U },
    { AIRLINE_PR, "PR 2526", "CGY", "MNL", "5:30 PM",  "7:10 PM",  "A321N",    7150U, 65800U },
    { AIRLINE_PR, "PR 2534", "CGY", "MNL", "7:35 PM",  "9:05 PM",  "A321N",    7100U, 65200U },
    { AIRLINE_PR, "PR 2528", "CGY", "MNL", "10:55 PM", "12:35 AM", "A320N",    6850U, 63400U },

    /* MNL -> CGY */
    { AIRLINE_PR, "PR 2521", "MNL", "CGY", "6:15 AM",  "7:55 AM",  "A321N",    7100U, 66200U },
    { AIRLINE_PR, "PR 2525", "MNL", "CGY", "2:45 PM",  "4:20 PM",  "A321N",    7100U, 66000U },
    { AIRLINE_PR, "PR 2533", "MNL", "CGY", "5:10 PM",  "6:45 PM",  "A321N",    7150U, 65600U },
    { AIRLINE_PR, "PR 2527", "MNL", "CGY", "8:20 PM",  "9:55 PM",  "A320N",    6850U, 63800U },

    /* CGY -> CEB */
    { AIRLINE_PR, "PR 1296", "CGY", "CEB", "6:30 AM",  "7:17 AM",  "A320N",    3800U, 62000U },
    { AIRLINE_PR, "PR 2316", "CGY", "CEB", "9:50 PM",  "10:40 PM", "A320-200", 3850U, 62500U },

    /* CEB -> CGY */
    { AIRLINE_PR, "PR 1295", "CEB", "CGY", "5:00 AM",  "5:50 AM",  "A320N",    3800U, 62000U },
    { AIRLINE_PR, "PR 2315", "CEB", "CGY", "8:30 PM",  "9:20 PM",  "A320-200", 3850U, 62500U },

    /* Thai Airways */
    { AIRLINE_TG, "TG 621", "MNL", "BKK", "1:34 PM", "3:50 PM",  "A330N", 18500U, 142000U },
    { AIRLINE_TG, "TG 617", "MNL", "BKK", "5:07 PM", "7:37 PM",  "A330N", 19200U, 145000U },
    { AIRLINE_TG, "TG 625", "MNL", "BKK", "7:50 PM", "10:20 PM", "A330N", 18900U, 143500U },
    { AIRLINE_TG, "TG 620", "BKK", "MNL", "8:15 AM", "12:39 PM", "A330N", 18400U, 141000U },
    { AIRLINE_TG, "TG 616", "BKK", "MNL", "11:04 AM", "3:34 PM", "A330N", 18800U, 144200U },
    { AIRLINE_TG, "TG 624", "BKK", "MNL", "2:18 PM", "6:47 PM",  "A330N", 19100U, 146100U }
};

#define SCHEDULED_FLIGHT_COUNT (sizeof(k_scheduled_flights) / sizeof(k_scheduled_flights[0]))

#ifdef __cplusplus
}
#endif

#endif /* AERODENS_ALLOWED_FLIGHTS_H */

