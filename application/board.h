#pragma once

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_err.h"

#define BOARD_I2C_PORT              I2C_NUM_0
#define BOARD_I2C_SDA_GPIO          GPIO_NUM_4
#define BOARD_I2C_SCL_GPIO          GPIO_NUM_5

#define BOARD_I2C_CLK_STRETCH_TICK  300U
#define BOARD_I2C_TIMEOUT_MS        100U
#define LED_GPIO                    GPIO_NUM_5
#define BUTTON_GPIO                 GPIO_NUM_4

#define BOARD_ADS1115_ADDRESS       0x48U
#define BOARD_SAMPLE_INTERVAL_MS    500U

esp_err_t sensor_power_init(void);

void reboot_soon(void);

void gpio_init_all(void);

bool take_sensor_reading(reading_t *out);

