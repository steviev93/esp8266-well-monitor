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
#include "../services/wifi_manager.h"
#include "../include/app_types.h"
#include "../services/nvs_service.h"

#include "buffer.h"
#include "supervisor.h"
#include "board.h"

#include "esp_task_wdt.h"


#define TELEMETRY_PAYLOAD_SIZE      128U

#define TELEMETRY_TASK_STACK_SIZE  4096U
#define TELEMETRY_TASK_PRIORITY    3U

void telemetry_task(void *parameter);



