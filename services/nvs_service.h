#pragma once

#include <stdint.h>
#include <string.h>
#include "../include/app_types.h"

#define DEFAULT_SAMPLE_SETTLE_MS     1000
#define DEFAULT_RETENTION_COUNT      96
#define NVS_READING_CAPACITY         512

esp_err_t nvs_store_pending_reading(const reading_t *reading);

esp_err_t nvs_service_initialize(void);

esp_err_t rebuild_nvs_reading_metadata();

esp_err_t load_nvs_reading(
    reading_t *out
);

esp_err_t pop_metadata();

esp_err_t get_latest_metadata(
    nvs_reading_metadata_t *out);

esp_err_t get_reading_store(
    reading_buffer_t *out);

void save_config(app_config_t *config);

bool load_wifi_credentials(char *ssid, char *password, size_t ssid_size, size_t pass_size);

esp_err_t  save_wifi_credentials(const char *ssid, const char *password);

bool get_latest_config(app_config_t *out);
