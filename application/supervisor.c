#include "supervisor.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "esp_log.h"

#include "../services/wifi_manager.h"
#include "buffer.h"

#define TELEMETRY_FAILURE_THRESHOLD     3U

#define HEARTBEAT_TIMEOUT_MS 10000U


static const char *TAG = "supervisor";

static supervisor_state_t supervisor_state =
    SUPERVISOR_STATE_OFFLINE;

static uint32_t consecutive_telemetry_failures = 0U;

static size_t buffered_sample_count = 0U;

static uint32_t last_heartbeat = 0U;

static uint32_t millis_now(void)
{
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

static const char *supervisor_state_to_string(void) {
    switch (supervisor_state) {
        case SUPERVISOR_STATE_HEALTHY:
            return "HEALTHY";
        
        case SUPERVISOR_STATE_DEGRADED:
            return "DEGRADED";

        case SUPERVISOR_STATE_OFFLINE:
            return "OFFLINE";

        case SUPERVISOR_STATE_FAULT:
            return "FAULT";
        
        default:
            return "UNKNOWN";
    }
 }

bool supervisor_is_task_healthy(void) {
    uint32_t now = millis_now();

    uint32_t elapsed = now - last_heartbeat;

    if (elapsed >= HEARTBEAT_TIMEOUT_MS) {
        return false;
    }

    return true;
}

static void supervisor_update_state(void) {
    if (telemetry_buffer_is_full()) {
        supervisor_state = SUPERVISOR_STATE_FAULT;
        ESP_LOGW(
            TAG,
            "%s: Telemetry buffer is full",
            supervisor_state_to_string()
        );
        return;
    }
    if (!wifi_manager_is_connected()) {
        supervisor_state = SUPERVISOR_STATE_OFFLINE;
        ESP_LOGW(
            TAG,
            "%s: Wi-Fi is disconnected",
            supervisor_state_to_string()
        );
        return;
    }
    if (consecutive_telemetry_failures >= TELEMETRY_FAILURE_THRESHOLD){
        supervisor_state = SUPERVISOR_STATE_DEGRADED;
        ESP_LOGW(
            TAG,
            "%s: %u failures is above threshold",
            supervisor_state_to_string(),
            (unsigned int)consecutive_telemetry_failures
        );
        return;
    }
    supervisor_state = SUPERVISOR_STATE_HEALTHY;
    ESP_LOGI(
        TAG,
        "%s: system functioning properly",
        supervisor_state_to_string()
    );
    
}

esp_err_t supervisor_init(void)
{
    consecutive_telemetry_failures = 0;
    buffered_sample_count = 0;

    supervisor_update_state();

    last_heartbeat = millis_now();

    return ESP_OK;
}

void supervisor_report_telemetry_result(esp_err_t result)
{
    if (result == ESP_OK) {
        consecutive_telemetry_failures = 0;
    } else {
        if (consecutive_telemetry_failures < TELEMETRY_FAILURE_THRESHOLD) {
            consecutive_telemetry_failures++;
        }
    }
    supervisor_update_state();
}

void supervisor_report_buffer_usage(size_t count)
{
    buffered_sample_count = count;
    supervisor_update_state();
}

supervisor_state_t supervisor_get_state(void)
{
    return supervisor_state;
}

void supervisor_report_heartbeat(void) {
    last_heartbeat = millis_now();
}

void supervisor_task(void *parameter) {
    (void)parameter;
    while (true) {
        bool result = supervisor_is_task_healthy();
        if (!result) {
            ESP_LOGI(
                TAG,
                "telemetry_task not healthy"
            );
        } else {
            ESP_LOGI(
                TAG,
                "telemetry_task healthy"
            );
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000U));
    }
    
    
}