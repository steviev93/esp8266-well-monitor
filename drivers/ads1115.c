#include "ads1115.h"

#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_log.h"

#include "board.h"
#include "hal/i2c_master.h"

#define ADS1115_REG_CONVERSION          0x00U
#define ADS1115_REG_CONFIG              0x01U

#define ADS1115_CONFIG_OS_START         0x8000U
#define ADS1115_CONFIG_MUX_AIN0_GND     0x4000U
#define ADS1115_CONFIG_MUX_AIN1_GND     0x5000U
#define ADS1115_CONFIG_MUX_AIN2_GND     0x6000U
#define ADS1115_CONFIG_MUX_AIN3_GND     0x7000U

#define ADS1115_CONFIG_PGA_4_096V       0x0200U
#define ADS1115_CONFIG_SINGLE_SHOT      0x0100U
#define ADS1115_CONFIG_DATA_RATE_128    0x0080U
#define ADS1115_CONFIG_COMP_DISABLE     0x0003U
#define ADS1115_CONFIG_READY_MASK       0x8000U

#define ADS1115_CONVERSION_TIMEOUT_MS   20U
#define ADS1115_POLL_INTERVAL_MS        1U

#define ADS1115_FULL_SCALE_VOLTS        4.096f
#define ADS1115_POSITIVE_COUNTS         32768.0f
#define ADS1115_CONFIG_A0_A1_SINGLESHOT  0x8383
#define ADS1115_LSB_VOLTS                0.000125f

/*
   ADS1115 config register, 0x8383:
   OS    = 1      Start single conversion
   MUX   = 000    AIN0 - AIN1 differential
   PGA   = 001    +/-4.096V range
   MODE  = 1      Single-shot mode
   DR    = 100    128 samples/sec
   COMP  = disabled

   For +/-4.096V range:
   LSB = 4.096 / 32768 = 0.000125 V
*/

// Recommended shunt value for 4-20mA loop into 3.3V-safe ADC range.


static const char *TAG = "ads1115";

static uint16_t ads1115_channel_mux(
    ads1115_channel_t channel
)
{
    switch (channel) {
        case ADS1115_CHANNEL_0:
            return ADS1115_CONFIG_MUX_AIN0_GND;
        case ADS1115_CHANNEL_1:
            return ADS1115_CONFIG_MUX_AIN1_GND;
        case ADS1115_CHANNEL_2:
            return ADS1115_CONFIG_MUX_AIN2_GND;

        case ADS1115_CHANNEL_3:
            return ADS1115_CONFIG_MUX_AIN3_GND;

        default:
            return 0U;

    }
}

esp_err_t ads1115_write_register(
    uint8_t register_address,
    uint16_t register_value
)
{
    uint8_t data[3] = {
        register_address,
        (uint8_t)(register_value >> 8U),
        (uint8_t)(register_value & 0xFFU)
    };

    return i2c_master_write_bytes(
        BOARD_ADS1115_ADDRESS,
        data,
        sizeof(data)
    );
}

esp_err_t ads1115_read_register(
    uint8_t register_address,
    uint16_t *register_value
)
{
    if (register_value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t received[2] = {0U, 0U};

    esp_err_t result = i2c_master_write_read(
        BOARD_ADS1115_ADDRESS,
        &register_address,
        1U,
        received,
        sizeof(received)
    );

    if (result != ESP_OK) {
        return result;
    }

    *register_value = ((uint16_t)received[0] << 8U) | (uint16_t)received[1];

    return ESP_OK;
}

esp_err_t ads1115_init(void)
{
    esp_err_t result = i2c_master_probe(
        BOARD_ADS1115_ADDRESS
    );

    if (result != ESP_OK) {
        ESP_LOGE(
            TAG,
            "ADS1115 did not respond at 0x%02X",
            BOARD_ADS1115_ADDRESS
        );
        return result;
    }

    uint16_t config_register = 0U;

    result = ads1115_read_register(
        ADS1115_REG_CONFIG,
        &config_register
    );

    if (result != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Unable to read configuration register: %s",
            esp_err_to_name(result)
        );

        return result;
    }

    ESP_LOGI(
        TAG,
        "ADS1115 found at 0x%02X",
        BOARD_ADS1115_ADDRESS
    );

    ESP_LOGI(
        TAG,
        "Initial configuration register: 0x%04X",
        config_register
    );

    return ESP_OK;
}

esp_err_t ads1115_read_single_ended(
    ads1115_channel_t channel,
    int16_t *raw_value
)
{
    if (raw_value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t mux = ads1115_channel_mux(channel);

    if (mux == 0U) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t configuration =
        ADS1115_CONFIG_OS_START |
        mux |
        ADS1115_CONFIG_PGA_4_096V |
        ADS1115_CONFIG_SINGLE_SHOT |
        ADS1115_CONFIG_DATA_RATE_128 |
        ADS1115_CONFIG_COMP_DISABLE;

    esp_err_t result = ads1115_write_register(
        ADS1115_REG_CONFIG,
        configuration
    );

    if (result != ESP_OK) {
        return result;
    }


    for (
        uint32_t elapsed_ms = 0U;
        elapsed_ms < ADS1115_CONVERSION_TIMEOUT_MS;
        elapsed_ms += ADS1115_POLL_INTERVAL_MS
    ) {
        uint16_t current_config = 0U;

        result = ads1115_read_register(
            ADS1115_REG_CONFIG,
            &current_config
        );

        if (result != ESP_OK) {
            return result;
        }

        if (
            (current_config & ADS1115_CONFIG_READY_MASK) != 0U
        ) {
            uint16_t conversion_register = 0U;

            result = ads1115_read_register(
                ADS1115_REG_CONVERSION,
                &conversion_register
            );

            if (result != ESP_OK) {
                return result;
            }

            *raw_value = (int16_t)conversion_register;

            return ESP_OK;
        }

        vTaskDelay(
            pdMS_TO_TICKS(ADS1115_POLL_INTERVAL_MS)
        );
    }

    ESP_LOGE(TAG, "ADS1115 conversion timed out");

    return ESP_ERR_TIMEOUT;
}

float ads1115_raw_to_voltage(int16_t raw_value) {
    return (
        (float)raw_value *
        ADS1115_FULL_SCALE_VOLTS
    ) / ADS1115_POSITIVE_COUNTS;
}

static bool ads1115_read_differential_a0_a1_raw(int16_t &raw, float &volts)
{
    if (!ads1115_write_register(ADS1115_REG_CONFIG, ADS1115_CONFIG_A0_A1_SINGLESHOT)) {
        ESP_LOGE(TAG, "ADS1115 config write failed");
        return false;
    }

    vTaskDelay(pdMS_TO_TICKS(20));

    uint16_t raw_u16 = 0;
    if (!ads1115_read_register(ADS1115_REG_CONV, raw_u16)) {
        ESP_LOGE(TAG, "ADS1115 conversion read failed");
        return false;
    }

    raw = (int16_t)raw_u16;
    volts = raw * ADS1115_LSB_VOLTS;

    ESP_LOGI(TAG, "ADS raw=%d", raw);

    return true;
}