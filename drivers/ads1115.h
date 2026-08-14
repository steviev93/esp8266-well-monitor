#pragma once

#include <stdint.h>
#include "esp_err.h"


typedef enum {
    ADS1115_CHANNEL_0 = 0,
    ADS1115_CHANNEL_1,
    ADS1115_CHANNEL_2,
    ADS1115_CHANNEL_3,
} ads1115_channel_t;

esp_err_t ads1115_init(void);

esp_err_t ads1115_read_register(
    uint8_t register_address,
    uint16_t *register_value
);

esp_err_t ads1115_write_register(
    uint8_t register_address,
    uint16_t register_value
);

esp_err_t ads1115_read_single_ended(
    ads1115_channel_t channel,
    int16_t *raw_value
);

float ads1115_raw_to_voltage(int16_t raw_value);