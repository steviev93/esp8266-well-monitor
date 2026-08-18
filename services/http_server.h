#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_http_server.h"

#include "../include/app_types.h"
#include "../application/board.h"
#include "nvs_service.h"

#define SSID_SIZE                    33
#define PASSWORD_SIZE                65
#define ITEM_SIZE                    384
#define MAX_REQUEST_BODY_SIZE        256

void start_http_server(bool setup_mode);
