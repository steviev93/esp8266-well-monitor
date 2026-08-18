#pragma once

#include "telemetry.h"
#include "../include/app_types.h"

void telemetry_buffer_initialize(void);

bool telemetry_buffer_is_empty(void);

bool telemetry_buffer_is_full(void);

void telemetry_buffer_push(const reading_t *sample);

bool telemetry_buffer_peek(reading_t *sample_out);

bool telemetry_buffer_pop(void);

size_t telemetry_buffer_count(void);



