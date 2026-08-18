#pragma once

#include <stdint.h>
#include "esp_err.h"

esp_err_t wifi_manager_initialize(void);

bool wifi_manager_is_connected(void);