#pragma once

#include <stdint.h>
#include "app_types.h"

#define DEFAULT_SAMPLE_SETTLE_MS     1000
#define DEFAULT_RETENTION_COUNT      96
#define NVS_READING_CAPACITY         512

void save_reading_store();

esp_err_t get_reading_store(
    reading_store_t *out);

static void load_config();

void save_config(app_config_t *config);

bool load_wifi_credentials(char *ssid, char *password, size_t ssid_size, size_t pass_size);

esp_err_t  save_wifi_credentials(const char *ssid, const char *password);

static void load_reading_store();

static void save_reading_store();

static void load_default_config();

static void clamp_config(app_config_t *config);

void add_reading(const reading_t *reading);

bool get_latest_reading(reading_type_t *out);

bool get_latest_config(app_config_t *out);
