#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "wifi_manager.h"

/*
   Well Monitor - ESP8266 NodeMCU, non-Arduino, ESP8266 RTOS SDK / ESP-IDF-style C++

   Behavior:
   - NodeMCU stays awake and hosts HTTP server continuously.
   - Sensor loop is OFF by default.
   - POST /sample turns IRLB8721 MOSFET ON, waits, reads ADS1115, turns MOSFET OFF.
   - GET /reading returns latest reading.
   - GET /readings returns retained readings.
   - GET /config returns config.
   - POST /config updates settings.
   - POST /wifi saves Wi-Fi credentials and reboots.
   - If no Wi-Fi credentials are stored, starts setup AP mode.

   NodeMCU pin plan:
   D2 / GPIO4  -> ADS1115 SDA
   D1 / GPIO5  -> ADS1115 SCL
   D5 / GPIO14 -> IRLB8721 gate control

   ADS1115 differential measurement:
   ADS A0 -> top of 150Ω shunt
   ADS A1 -> bottom of 150Ω shunt / IRLB8721 drain

   Current loop wiring:
   12V+ -> fuse -> pressure sensor +
   pressure sensor - -> top of 150Ω shunt
   bottom of 150Ω shunt -> IRLB8721 drain
   IRLB8721 source -> system ground

   Gate wiring:
   GPIO14 / D5 -> 100Ω to 1k resistor -> IRLB8721 gate
   IRLB8721 gate -> 10k pulldown -> GND

   Important:
   Do not put ADS A1 on MOSFET source if the shunt is above the MOSFET.
   A1 should go to the bottom of the shunt, which is also the MOSFET drain.
*/

static const char *TAG = "well_monitor_nodemcu";




/* ============================================================
   Utility
   ============================================================ */

static uint32_t millis_now()
{
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

/* ============================================================
   app_main
   ============================================================ */

extern "C" void app_main()
{
    ESP_LOGI(TAG, "Booting always-on well monitor");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    load_config();
    load_reading_store();
    gpio_init_all();
    wifi_manager_initialize();

    // Take one reading shortly after boot if connected to Wi-Fi.
    if (g_wifi_connected) {
        xTaskCreate(startup_sample_task, "startup_sample_task", 4096, nullptr, 5, nullptr);
    }

    ESP_LOGI(TAG, "System ready");
}
