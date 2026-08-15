#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"

#include "esp_http_client.h"
#include "wifi_manager.h"

#define TELEMETRY_PAYLOAD_SIZE      128U

#define TELEMETRY_TASK_STACK_SIZE  4096U
#define TELEMETRY_TASK_PRIORITY    5U

void telemetry_task(void *parameter);

static esp_err_t send_telemetry_sample(
    const reading_t *sample);

static esp_err_t  replay_buffered_telemetry(void);

static esp_err_t  replay_stored_telemetry(void);



