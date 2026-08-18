#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "../services/wifi_manager.h"
#include "../application/board.h"
#include "../application/telemetry.h"

static const char *TAG = "well_monitor_nodemcu";

void app_main()
{
    BaseType_t task_created;

    ESP_LOGI(TAG, "Booting always-on well monitor");

    gpio_init_all();
    esp_err_t nvs_init = nvs_service_initialize();

    if (nvs_init != ESP_OK) {
        ESP_LOGE(
            TAG,
            "nvs_init failed: %s",
            esp_err_to_name(nvs_init)
        );
        return;
    }

    esp_err_t result = wifi_manager_initialize();
    if (result != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Error initializing Wi-Fi: \n%s",
            esp_err_to_name(result)
        );
    }

    task_created = xTaskCreate(
        telemetry_task, 
        "telemetry_task", 
        TELEMETRY_TASK_STACK_SIZE, 
        NULL, 
        TELEMETRY_TASK_PRIORITY, 
        NULL
    );

    if (task_created != pdPASS) {
        ESP_LOGE(
            TAG,
            "telemetry_task failed to create"
        );
        return;
    }

    ESP_LOGI(TAG, "System ready");
}
