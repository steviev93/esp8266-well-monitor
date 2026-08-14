#pragma once

#include <stddef.h>
#include <stdint.h>

#include "esp_err.h"

esp_err_t i2c_master_bus_init(void);

esp_err_t i2c_master_bus_deinit(void);

esp_err_t i2c_master_probe(uint8_t device_address);

esp_err_t i2c_master_write_bytes(
    uint8_t device_address,
    const uint8_t *data,
    size_t data_length
);

esp_err_t i2c_master_write_read(
    uint8_t device_address,
    const uint8_t *write_data,
    size_t write_length,
    uint8_t *read_data,
    size_t read_length 
);