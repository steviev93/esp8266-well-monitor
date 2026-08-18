#pragma once
#include <stdint.h>
#define MAX_READINGS                 256
#define TELEMETRY_BUFFER_CAPACITY    256

typedef struct {
    uint32_t sequence;
    uint32_t uptime_ms;

    int16_t ads_raw;

    int32_t shunt_uV;
    int32_t shunt_mV;
    int32_t loop_uA;

    int32_t normalized_permille;
    int32_t level_percent_x100;
    int32_t water_level_mm;
} reading_t;

typedef struct {
    uint32_t count;
    uint32_t head;
    uint32_t tail;
} nvs_reading_metadata_t;

typedef struct {
    uint32_t sample_settle_ms;
    uint32_t retention_count;
} app_config_t;

typedef struct {
    uint32_t tail;
    uint32_t count;
    uint32_t head;
    reading_t readings[MAX_READINGS];
} reading_buffer_t;