#pragma once

#include <string.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_err.h"

#include "driver/gpio.h"
#include "../services/nvs_service.h"
#include "../hal/i2c_master.h"
#include "../drivers/ads1115.h"
#include "../include/app_types.h"

#define BOARD_I2C_PORT              I2C_NUM_0
#define BOARD_I2C_SDA_GPIO          GPIO_NUM_4
#define BOARD_I2C_SCL_GPIO          GPIO_NUM_5

#define BOARD_I2C_CLK_STRETCH_TICK  300U
#define BOARD_I2C_TIMEOUT_MS        100U
#define LED_GPIO                    GPIO_NUM_12

#define BOARD_ADS1115_ADDRESS       0x48U
#define BOARD_SAMPLE_INTERVAL_MS    500U

esp_err_t sensor_power_init(void);

void reboot_soon(void);

void gpio_init_all(void);

bool take_sensor_reading(reading_t *out);

