#pragma once

#include <stdint.h>
#include "esp_err.h"

esp_err_t wifi_manager_initialize(void);

static bool wifi_manager_is_connected(void);

static esp_err_t start_station_mode(const char *ssid, const char *password);

static void configure_static_ip(void);

static void start_setup_ap_mode(void);

static void start_station_mode(const char *ssid, const char *password);

static void wifi_event_handler(
    void *handler_argument,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data);

static esp_err_t wifi_init_common();