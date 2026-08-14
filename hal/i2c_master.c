#include "i2c_master.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"

#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_log.h"

#include "board.h"

#define I2C_ADDRESS_WRITE(address) \
    ((uint8_t)(((address) << 1U) | I2C_MASTER_WRITE))

#define I2C_ADDRESS_READ(address) \
    ((uint8_t)(((address) << 1U) | I2C_MASTER_READ))

static const char *TAG = "i2c_master";

static bool s_i2c_initialized = false;

static TickType_t i2c_timeout_ticks(void)
{
    TickType_t ticks = pdMS_TO_TICKS(BOARD_I2C_TIMEOUT_MS);

    /*
     * Protect against configurations where the timeout is shorter
     * than one RTOS tick.
     */
    if (ticks == 0U) {
        ticks = 1U;
    }

    return ticks;
}

esp_err_t i2c_master_bus_init(void)
{
    if (s_i2c_initialized) {
        ESP_LOGW(TAG, "I2C bus is already initialized");
        return ESP_OK;
    }

    esp_err_t result = i2c_driver_install(
        BOARD_I2C_PORT,
        I2C_MODE_MASTER
    );

    if (result != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Unable to install I2C driver: %s",
            esp_err_to_name(result)
        );

        return result;
    }

    i2c_config_t config = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = BOARD_I2C_SDA_GPIO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = BOARD_I2C_SCL_GPIO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .clk_stretch_tick = BOARD_I2C_CLK_STRETCH_TICK
    };

    result = i2c_param_config(
        BOARD_I2C_PORT,
        &config
    );

    if (result != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Unable to configure I2C bus: %s",
            esp_err_to_name(result)
        );

        i2c_driver_delete(BOARD_I2C_PORT);

        return result;
    }

    s_i2c_initialized = true;

    ESP_LOGI(
        TAG,
        "I2C initialized: SDA=GPIO%d, SCL=GPIO%d",
        BOARD_I2C_SDA_GPIO,
        BOARD_I2C_SCL_GPIO
    );

    return ESP_OK;
}

esp_err_t i2c_master_bus_deinit(void)
{
    if (!s_i2c_initialized) {
        return ESP_OK;
    }

    esp_err_t result = i2c_driver_delete(
        BOARD_I2C_PORT
    );

    if (result == ESP_OK) {
        s_i2c_initialized = false;
        ESP_LOGI(TAG, "I2C driver deleted");
    }

    return result;
}

esp_err_t i2c_master_probe(uint8_t device_address)
{
    if (!s_i2c_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (device_address > 0x7FU) {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_cmd_handle_t command = i2c_cmd_link_create();

    if (command == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t result = i2c_master_start(command);

    if (result == ESP_OK) {
        result = i2c_master_write_byte(
            command,
            I2C_ADDRESS_WRITE(device_address),
            true
        );
    }

    if (result == ESP_OK) {
        result = i2c_master_stop(command);
    }

    if (result == ESP_OK) {
        result = i2c_master_cmd_begin(
            BOARD_I2C_PORT,
            command,
            i2c_timeout_ticks()
        );
    }

    i2c_cmd_link_delete(command);

    return result;
}

esp_err_t i2c_master_write_bytes(
    uint8_t device_address,
    const uint8_t *data,
    size_t data_length
)
{
    if (!s_i2c_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (
        device_address > 0x7FU ||
        data == NULL ||
        data_length == 0U
    ) {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_cmd_handle_t command = i2c_cmd_link_create();

    if (command == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t result = i2c_master_start(command);

    if (result == ESP_OK) {
        result = i2c_master_write_byte(
            command,
            I2C_ADDRESS_WRITE(device_address),
            true
        );
    }

    if (result == ESP_OK) {
        result = i2c_master_write(
            command,
            (uint8_t *)data,
            data_length,
            true
        );
    }

    if (result == ESP_OK) {
        result = i2c_master_stop(command);
    }

    if (result == ESP_OK) {
        result = i2c_master_cmd_begin(
            BOARD_I2C_PORT,
            command,
            i2c_timeout_ticks()
        );
    }

    i2c_cmd_link_delete(command);

    return result;
}

esp_err_t i2c_master_write_read(
    uint8_t device_address,
    const uint8_t *write_data,
    size_t write_length,
    uint8_t *read_data,
    size_t read_length
)
{
    if (!s_i2c_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (
        device_address > 0x7FU ||
        write_data == NULL ||
        write_length == 0U ||
        read_data == NULL ||
        read_length == 0U
    ) {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_cmd_handle_t command = i2c_cmd_link_create();

    if (command == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t result = i2c_master_start(command);

    if (result == ESP_OK) {
        result = i2c_master_write_byte(
            command,
            I2C_ADDRESS_WRITE(device_address),
            true
        );
    }

    if (result == ESP_OK) {
        result = i2c_master_write(
            command,
            (uint8_t *)write_data,
            write_length,
            true
        );
    }

    /*
     * Calling start again generates a repeated-start condition.
     */
    if (result == ESP_OK) {
        result = i2c_master_start(command);
    }

    if (result == ESP_OK) {
        result = i2c_master_write_byte(
            command,
            I2C_ADDRESS_READ(device_address),
            true
        );
    }

    if (result == ESP_OK) {
        result = i2c_master_read(
            command,
            read_data,
            read_length,
            I2C_MASTER_LAST_NACK
        );
    }

    if (result == ESP_OK) {
        result = i2c_master_stop(command);
    }

    if (result == ESP_OK) {
        result = i2c_master_cmd_begin(
            BOARD_I2C_PORT,
            command,
            i2c_timeout_ticks()
        );
    }

    i2c_cmd_link_delete(command);

    return result;
}