#include "esp_log.h"
#include "esp_err.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "nvs_service.h"

static app_config_t g_config;
static nvs_reading_metadata_t metadata;

static const char *TAG = "nvs_service";

static void clamp_config(app_config_t *config)
{
    if (config->sample_settle_ms < 100) {
        config->sample_settle_ms = 100;
    }

    if (config->sample_settle_ms > 10000) {
        config->sample_settle_ms = 10000;
    }

    if (config->retention_count < 1) {
        config->retention_count = 1;
    }

    if (config->retention_count > MAX_READINGS) {
        config->retention_count = MAX_READINGS;
    }
}

static void load_default_config()
{
    g_config.sample_settle_ms = DEFAULT_SAMPLE_SETTLE_MS;
    g_config.retention_count = DEFAULT_RETENTION_COUNT;
}

static void load_config()
{
    load_default_config();

    nvs_handle handle;

    esp_err_t err =
        nvs_open("well", NVS_READONLY, &handle);

    if (err != ESP_OK) {
        ESP_LOGW(
            TAG,
            "No config namespace yet; using defaults"
        );

        return;
    }

    size_t len = sizeof(g_config);

    err = nvs_get_blob(
        handle,
        "config",
        &g_config,
        &len
    );

    nvs_close(handle);

    if (err != ESP_OK ||
        len != sizeof(g_config)) {

        ESP_LOGW(
            TAG,
            "No valid config found; using defaults"
        );

        load_default_config();
    }

    clamp_config(&g_config);
}

esp_err_t nvs_store_pending_reading(const reading_t *reading)
{
    if (reading == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (metadata.count >= NVS_READING_CAPACITY) {
        ESP_LOGE(TAG, "Persistent telemetry backlog full");
        return ESP_ERR_NO_MEM;
    }

    nvs_handle handle;
    char key[16];

    snprintf(
        key,
        sizeof(key),
        "r%03u",
        metadata.head
    );

    esp_err_t err = nvs_open(
        "telemetry",
        NVS_READWRITE,
        &handle
    );

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for reading");
        return err;
    }

    err = nvs_set_blob(
        handle,
        key,
        reading,
        sizeof(*reading)
    );

    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);

    if (err != ESP_OK) {
        return err;
    }

    metadata.head =
        (metadata.head + 1) % NVS_READING_CAPACITY;

    metadata.count++;

    

    return ESP_OK;
}

esp_err_t nvs_service_initialize(void)
{
    esp_err_t err;

    err = nvs_flash_init();

    if (err != ESP_OK) {
        return err;
    }

    load_config();

    rebuild_nvs_reading_metadata();

    return ESP_OK;
}
    //TODO

esp_err_t rebuild_nvs_reading_metadata() {

    metadata.count = 0;
    metadata.head = 0;
    metadata.tail = 0;

    nvs_handle handle;

    esp_err_t err = nvs_open(
        "telemetry",
        NVS_READONLY,
        &handle
    );

    if (err != ESP_OK) {
        return err;
    }

    bool found_any = false;

    uint32_t smallest_sequence = UINT32_MAX;
    uint32_t largest_sequence = 0;

    uint32_t smallest_index = 0;
    uint32_t largest_index = 0;

    for (uint32_t i = 0; i < NVS_READING_CAPACITY; i++) {

        char key[16];

        snprintf(
            key,
            sizeof(key),
            "r%03u",
            i
        );

        reading_t reading;
        size_t len = sizeof(reading);

        err = nvs_get_blob(
            handle,
            key,
            &reading,
            &len
        );

        if (err == ESP_ERR_NVS_NOT_FOUND) {
            continue;
        }

        if (err != ESP_OK) {
            nvs_close(handle);
            return err;
        }

        if (len != sizeof(reading)) {
            nvs_close(handle);
            return ESP_ERR_INVALID_SIZE;
        }

        metadata.count++;

        if (!found_any || reading.sequence < smallest_sequence) {
            smallest_sequence = reading.sequence;
            smallest_index = i;
        }

        if (!found_any || reading.sequence > largest_sequence) {
            largest_sequence = reading.sequence;
            largest_index = i;
        }

        found_any = true;
    }

    nvs_close(handle);

    if (!found_any) {
        metadata.head = 0;
        metadata.tail = 0;
        metadata.count = 0;

        return ESP_OK;
    }

    metadata.tail = smallest_index;

    metadata.head = (largest_index + 1) % NVS_READING_CAPACITY;

    return ESP_OK;
    
}

void save_config(app_config_t *config)
{
    clamp_config(config);

    nvs_handle handle;
    esp_err_t err = nvs_open("well", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for config");
        return;
    }

    nvs_set_blob(handle, "config", config, sizeof(*config));
    nvs_commit(handle);
    nvs_close(handle);
}

bool load_wifi_credentials(
    char *ssid,
    char *password,
    size_t ssid_size,
    size_t pass_size)
{
    if (ssid == NULL ||
        password == NULL ||
        ssid_size == 0 ||
        pass_size == 0) {
        return false;
    }

    nvs_handle handle;

    esp_err_t err =
        nvs_open("wifi", NVS_READONLY, &handle);

    if (err != ESP_OK) {
        return false;
    }

    size_t ssid_len = ssid_size;
    size_t pass_len = pass_size;

    err = nvs_get_str(
        handle,
        "ssid",
        ssid,
        &ssid_len
    );

    if (err != ESP_OK) {
        nvs_close(handle);
        return false;
    }

    err = nvs_get_str(
        handle,
        "password",
        password,
        &pass_len
    );

    if (err != ESP_OK) {
        password[0] = '\0';
    }

    nvs_close(handle);

    return ssid[0] != '\0';
}

esp_err_t load_nvs_reading(
    reading_t *out
) {
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle handle;

    esp_err_t err = nvs_open(
        "telemetry",
        NVS_READONLY,
        &handle
    );

    if (err != ESP_OK) {
        return err;
    }

    char key[16];

    snprintf(
        key,
        sizeof(key),
        "r%03u",
        metadata.tail
    );

    size_t len = sizeof(*out);

    err = nvs_get_blob(
        handle,
        key,
        out,
        &len
    );

    nvs_close(handle);

    if (err != ESP_OK) {
        return err;
    }

    if (len != sizeof(*out)) {
        return ESP_ERR_INVALID_SIZE;
    }

    return ESP_OK;
}

esp_err_t pop_metadata() {
    if (metadata.count == 0) {
        return ESP_ERR_NOT_FOUND;
    }

    char key[16];

    snprintf(
        key,
        sizeof(key),
        "r%03u",
        metadata.tail
    );

    nvs_handle handle;

    esp_err_t err = nvs_open(
        "telemetry",
        NVS_READWRITE,
        &handle
    );

    if (err != ESP_OK) {
        return err;
    }

    err = nvs_erase_key(handle, key);

    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);

    if (err != ESP_OK) {
        return err;
    }

    metadata.tail = (metadata.tail + 1) % NVS_READING_CAPACITY;

    metadata.count--;

    return ESP_OK;
}

esp_err_t save_wifi_credentials(
    const char *ssid,
    const char *password)
{
    if (ssid == NULL || ssid[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle handle;

    esp_err_t err =
        nvs_open("wifi", NVS_READWRITE, &handle);

    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Failed to open NVS for Wi-Fi credentials"
        );

        return err;
    }

    err = nvs_set_str(handle, "ssid", ssid);

    if (err == ESP_OK) {
        err = nvs_set_str(
            handle,
            "password",
            password ? password : ""
        );
    }

    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);

    return err;
}

bool get_latest_config(app_config_t *out)
{

    if (out == NULL) {
        return false;
    }

    *out = g_config;
    return true;
}

esp_err_t get_latest_metadata(
    nvs_reading_metadata_t *out)
{
    if (out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    *out = metadata;
    return ESP_OK;
}