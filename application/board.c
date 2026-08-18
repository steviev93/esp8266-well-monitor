#include "board.h"

#define PIN_SENSOR_ENABLE    GPIO_NUM_14  // NodeMCU D5
#define SENSOR_ENABLE_ACTIVE_LEVEL  1
#define SENSOR_ENABLE_IDLE_LEVEL    0
#define CAL_ZERO_UA 4638
#define CAL_NUMERATOR_MM 76
#define CAL_DENOMINATOR_UA 194

#define SENSOR_FULL_SCALE_WATER_MM 6000

#define SHUNT_OHMS_INT 150

static const char *TAG = "board";

static uint32_t millis_now(void)
{
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

esp_err_t sensor_enable(bool enabled)
{
    esp_err_t result;
    result = gpio_set_level(
        PIN_SENSOR_ENABLE,
        enabled ? SENSOR_ENABLE_ACTIVE_LEVEL : SENSOR_ENABLE_IDLE_LEVEL
    );
    return result;
}

void reboot_soon()
{
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
}

void gpio_init_all(void)
{
    gpio_config_t out_conf;
    memset(&out_conf, 0, sizeof(out_conf));

    out_conf.pin_bit_mask =
        (1ULL << PIN_SENSOR_ENABLE);

    out_conf.mode = GPIO_MODE_OUTPUT;
    out_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    out_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
    out_conf.intr_type = GPIO_INTR_DISABLE;

    gpio_config(&out_conf);

    sensor_enable(false);

    esp_err_t result = i2c_master_bus_init();

    if (result != ESP_OK) {
        ESP_LOGE(
            TAG,
            "I2C initialization failed: %s",
            esp_err_to_name(result)
        );

        return;
    }

    result = ads1115_init();

    if (result != ESP_OK) {
        ESP_LOGE(
            TAG,
            "ADS1115 initialization failed: %s",
            esp_err_to_name(result)
        );
    }
}


bool take_sensor_reading(reading_t *out)
{

    if (out == NULL) {
        return false;
    }
    
    ESP_LOGI(TAG, "Turning sensor loop ON");
    sensor_enable(true);

    app_config_t config;

    if (!get_latest_config(&config)) {
        ESP_LOGE(TAG, "Unable to load application config");
        sensor_enable(false);
        return false;
    }

    vTaskDelay(
        pdMS_TO_TICKS(config.sample_settle_ms)
    );

    float volts_unused = 0.0f;
    int16_t raw = 0;
    bool ok = ads1115_read_differential_a0_a1_raw(&raw, &volts_unused);

    sensor_enable(false);
    ESP_LOGI(TAG, "Turning sensor loop OFF");

    if (!ok) {
        return false;
    }

    /*
       ADS1115 at +/-4.096V:
       1 count = 0.000125 V = 125 microvolts

       shunt_uV = raw * 125
       loop_uA  = shunt_uV / 150 ohms

       Calibration currently uses measured bucket values instead of assuming
       perfect 4.000 mA zero and 20.000 mA full scale.

       Current calibration:
       - 5188 uA ≈ 216 mm
       - 5382 uA ≈ 292 mm

       This gives:
       - slope ≈ 76 mm / 194 uA
       - zero ≈ 4638 uA
    */

    int32_t shunt_uV = (int32_t)raw * 125;
    int32_t shunt_mV = shunt_uV / 1000;
    int32_t loop_uA = shunt_uV / SHUNT_OHMS_INT;

    /*
       Direct calibrated water depth calculation:

       water_level_mm =
           (loop_uA - CAL_ZERO_UA) * (76 mm / 194 uA)

       This avoids first mapping to 4-20mA percent, which was causing
       bad readings because your sensor has a real zero offset.
    */

    int32_t water_level_mm =
        ((loop_uA - CAL_ZERO_UA) * CAL_NUMERATOR_MM) / CAL_DENOMINATOR_UA;

    if (water_level_mm < 0) {
        water_level_mm = 0;
    }

    if (water_level_mm > SENSOR_FULL_SCALE_WATER_MM) {
        water_level_mm = SENSOR_FULL_SCALE_WATER_MM;
    }

    int32_t normalized_permille =
        (water_level_mm * 1000) / SENSOR_FULL_SCALE_WATER_MM;

    if (normalized_permille < 0) {
        normalized_permille = 0;
    }

    if (normalized_permille > 1000) {
        normalized_permille = 1000;
    }

    int32_t level_percent_x100 = normalized_permille * 10;

    memset(out, 0, sizeof(*out));

    out->uptime_ms = millis_now();
    out->ads_raw = raw;
    out->shunt_uV = shunt_uV;
    out->shunt_mV = shunt_mV;
    out->loop_uA = loop_uA;
    out->normalized_permille = normalized_permille;
    out->level_percent_x100 = level_percent_x100;
    out->water_level_mm = water_level_mm;

    ESP_LOGI(
        TAG,
        "Reading: raw=%d, shunt=%ld uV, loop=%ld uA, level=%ld.%02ld%%, water=%ld mm",
        raw,
        (long)shunt_uV,
        (long)loop_uA,
        (long)(level_percent_x100 / 100),
        (long)(level_percent_x100 % 100),
        (long)water_level_mm
    );

    return true;
}