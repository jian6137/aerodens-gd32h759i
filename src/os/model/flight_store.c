/*!
    \file    src/os/model/flight_store.c
    \brief   In-Memory Flight Record Store Implementation
*/

#include "flight_store.h"
#include <string.h>
#include <stdio.h>

static flight_record_t s_flights[FLIGHT_STORE_MAX_FLIGHTS];
static uint32_t s_flight_count = 0U;

void flight_store_init(void)
{
    s_flight_count = 0U;

    flight_record_t f1;
    memset(&f1, 0, sizeof(f1));
    strncpy(f1.flight_no, "5J-561", sizeof(f1.flight_no) - 1U);
    strncpy(f1.origin, "MNL", sizeof(f1.origin) - 1U);
    strncpy(f1.dest, "CEB", sizeof(f1.dest) - 1U);
    strncpy(f1.aircraft_type, "A320neo", sizeof(f1.aircraft_type) - 1U);
    strncpy(f1.depart_time, "8:00 PM", sizeof(f1.depart_time) - 1U);
    strncpy(f1.arrive_time, "9:45 PM", sizeof(f1.arrive_time) - 1U);
    f1.base_zfw_kg = 65200U;
    f1.planned_fuel_l = 6800U;
    f1.saf_blend_pct = 40U;
    f1.is_completed = false;
    flight_store_add(&f1);

    flight_record_t f2;
    memset(&f2, 0, sizeof(f2));
    strncpy(f2.flight_no, "5J-383", sizeof(f2.flight_no) - 1U);
    strncpy(f2.origin, "MNL", sizeof(f2.origin) - 1U);
    strncpy(f2.dest, "CGY", sizeof(f2.dest) - 1U);
    strncpy(f2.aircraft_type, "A320neo", sizeof(f2.aircraft_type) - 1U);
    strncpy(f2.depart_time, "4:10 AM", sizeof(f2.depart_time) - 1U);
    strncpy(f2.arrive_time, "5:40 AM", sizeof(f2.arrive_time) - 1U);
    f2.base_zfw_kg = 64600U;
    f2.planned_fuel_l = 6900U;
    f2.saf_blend_pct = 30U;
    f2.is_completed = false;
    flight_store_add(&f2);

    flight_record_t f3;
    memset(&f3, 0, sizeof(f3));
    strncpy(f3.flight_no, "TG-621", sizeof(f3.flight_no) - 1U);
    strncpy(f3.origin, "MNL", sizeof(f3.origin) - 1U);
    strncpy(f3.dest, "BKK", sizeof(f3.dest) - 1U);
    strncpy(f3.aircraft_type, "A330neo", sizeof(f3.aircraft_type) - 1U);
    strncpy(f3.depart_time, "1:34 PM", sizeof(f3.depart_time) - 1U);
    strncpy(f3.arrive_time, "3:50 PM", sizeof(f3.arrive_time) - 1U);
    f3.base_zfw_kg = 142000U;
    f3.planned_fuel_l = 18500U;
    f3.saf_blend_pct = 35U;
    f3.is_completed = false;
    flight_store_add(&f3);

    flight_record_t f4;
    memset(&f4, 0, sizeof(f4));
    strncpy(f4.flight_no, "TG-620", sizeof(f4.flight_no) - 1U);
    strncpy(f4.origin, "BKK", sizeof(f4.origin) - 1U);
    strncpy(f4.dest, "MNL", sizeof(f4.dest) - 1U);
    strncpy(f4.aircraft_type, "A330neo", sizeof(f4.aircraft_type) - 1U);
    strncpy(f4.depart_time, "8:15 AM", sizeof(f4.depart_time) - 1U);
    strncpy(f4.arrive_time, "12:39 PM", sizeof(f4.arrive_time) - 1U);
    f4.base_zfw_kg = 141000U;
    f4.planned_fuel_l = 18400U;
    f4.saf_blend_pct = 40U;
    f4.is_completed = false;
    flight_store_add(&f4);
}

uint32_t flight_store_get_count(void)
{
    return s_flight_count;
}

const flight_record_t *flight_store_get(uint32_t index)
{
    if (index >= s_flight_count) {
        return NULL;
    }
    return &s_flights[index];
}

bool flight_store_add(const flight_record_t *flight)
{
    if (flight == NULL || s_flight_count >= FLIGHT_STORE_MAX_FLIGHTS) {
        return false;
    }

    memcpy(&s_flights[s_flight_count], flight, sizeof(flight_record_t));
    s_flight_count++;
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
                     "%s{\"flight_no\":\"%s\",\"origin\":\"%s\",\"dest\":\"%s\","
                     "\"aircraft\":\"%s\",\"depart\":\"%s\",\"arrive\":\"%s\","
                     "\"zfw_kg\":%lu,\"fuel_l\":%lu,\"saf_pct\":%u}",
                     (i > 0U) ? "," : "",
                     f->flight_no, f->origin, f->dest,
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

