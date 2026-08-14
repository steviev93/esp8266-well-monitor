#pragma once

#include <stddef.h>
#include <stdbool.h>

#include "esp_err.h"

#define SUPERVISOR_TASK_STACK_SIZE  2048U
#define SUPERVISOR_TASK_PRIORITY    6U

typedef enum
{
    SUPERVISOR_STATE_HEALTHY = 0,
    SUPERVISOR_STATE_DEGRADED,
    SUPERVISOR_STATE_OFFLINE,
    SUPERVISOR_STATE_FAULT
} supervisor_state_t;

/*
 * Initialize supervisor state.
 */
esp_err_t supervisor_init(void);

/*
 * Tell the supervisor whether the most recent
 * telemetry transmission succeeded.
 */
void supervisor_report_telemetry_result(esp_err_t result);

void supervisor_report_heartbeat(void);

/*
 * Tell the supervisor how many entries are currently
 * stored in the offline buffer.
 */
void supervisor_report_buffer_usage(size_t count);

/*
 * Return the supervisor's current assessment
 * of overall system health.
 */
supervisor_state_t supervisor_get_state(void);

bool supervisor_is_task_healthy(void);

void supervisor_task(void *parameter);
