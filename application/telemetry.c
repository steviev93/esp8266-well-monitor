#include "telemetry.h"

#define TELEMETRY_URL               "http://httpbin.org/post"

#define SAMPLE_PERIOD_MS            5000U

#define TELEMETRY_PAYLOAD_SIZE      128U

static const char *TAG = "telemetry";

static esp_err_t send_telemetry_sample(
    const reading_t *sample)
{
    if (sample == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    char payload[384];
    int result = snprintf(
        payload,
        sizeof(payload),
        "{"
        "\"ok\":true,"
        "\"sequence\":%u,"
        "\"ads_raw\":%d,"
        "\"shunt_uV\":%ld,"
        "\"shunt_mV\":%ld,"
        "\"loop_uA\":%ld,"
        "\"normalized_permille\":%ld,"
        "\"level_percent_x100\":%ld,"
        "\"water_level_mm\":%ld"
        "}",
        sample->sequence,
        sample->ads_raw,
        (long)sample->shunt_uV,
        (long)sample->shunt_mV,
        (long)sample->loop_uA,
        (long)sample->normalized_permille,
        (long)sample->level_percent_x100,
        (long)sample->water_level_mm
    );

    if (result < 0 || (size_t)result >= sizeof(payload)) {
        ESP_LOGE(
            TAG,
            "Error forming payload"
        );
        return ESP_FAIL;
    }
    
    esp_http_client_config_t client_config = {
        .url = TELEMETRY_URL
    };
    esp_http_client_handle_t client_handle = esp_http_client_init(&client_config);
    if (client_handle == NULL) {
        ESP_LOGE(
            TAG,
            "ESP HTTP init failed"
        );
        return ESP_FAIL;
    }

    esp_http_client_set_method(client_handle, HTTP_METHOD_POST);
    esp_http_client_set_header(client_handle, "Content-Type", "application/json");
    
    esp_http_client_set_post_field(client_handle, payload, result);

    esp_err_t result_client = esp_http_client_perform(client_handle);

    int stat_code = esp_http_client_get_status_code(client_handle);
    
    if (result_client == ESP_OK &&
        stat_code >= 200
        &&
        stat_code < 300) {
        
        ESP_LOGI(
            TAG,
            "sensor value: %d, status code: %d",
            sample->ads_raw,
            stat_code
        );
    } else {
        ESP_LOGE(
            TAG,
            "HTTP request failed: %s",
            esp_err_to_name(result_client)
        );
        result_client = ESP_FAIL;
    }

    esp_http_client_cleanup(client_handle);


    return result_client;
}

static esp_err_t replay_buffered_telemetry(void)
{
    while (!telemetry_buffer_is_empty()) {

        reading_t sample;
        esp_err_t send_result;

        bool sample_exists = telemetry_buffer_peek(&sample);
        
        if (sample_exists) {
            ESP_LOGI(
                TAG,
                "replaying seq: %u",
                sample.sequence
            );
            send_result = send_telemetry_sample(&sample);
            if (send_result != ESP_OK) {
                ESP_LOGW(
                    TAG,
                    "seq: %u FAILED. stopping.",
                    sample.sequence
                );
                return ESP_FAIL;
            } else {
                bool pop_r = telemetry_buffer_pop();

                if (pop_r) {
                    ESP_LOGI(
                        TAG,
                        "Replay succeded!\nBuffered samples remaining: %d",
                        (unsigned int)telemetry_buffer_count()
                    );
                }
            }
        }
    }
    return ESP_OK;
}

static esp_err_t replay_stored_telemetry(void)
{
    
    while (true) {
        reading_t reading;
        esp_err_t err = load_nvs_reading(&reading);

        if (err == ESP_ERR_NOT_FOUND) {
            return ESP_OK;
        }

        if (err != ESP_OK) {
            ESP_LOGE(
                TAG,
                "Failed to load stored telemetry: %s",
                esp_err_to_name(err)
            );

            return err;
        }
        esp_err_t send_result;
        ESP_LOGI(
            TAG,
            "replaying seq: %u",
            reading.sequence
        );
        send_result = send_telemetry_sample(&reading);
        if (send_result != ESP_OK) {
            ESP_LOGW(
                TAG,
                "seq: %u FAILED. stopping.",
                reading.sequence
            );
            return ESP_FAIL;
        } else {
            esp_err_t result = pop_metadata();
            nvs_reading_metadata_t meta;
            get_latest_metadata(&meta);
            if (result == ESP_OK) {
                ESP_LOGI(
                    TAG,
                    "Replay succeded!\nStored readings remaining: %d",
                    (unsigned int)meta.count
                );
                return result;
            }
        }
    }
}

void telemetry_task(void *parameter)
{
    (void)parameter;
    esp_err_t result;
    reading_t sample;
    uint32_t sequence_num = 0;

    esp_err_t err = esp_task_wdt_init();

    if (err != ESP_OK) {
        ESP_LOGE(
            TAG, 
            "Failed to initialize task watchdog: %s\nRebooting",
            esp_err_to_name(err)
        );
        reboot_soon();
    }

    while (true) {
        
        if (!take_sensor_reading(&sample)) {
            ESP_LOGE(
                TAG,
                "Failed to grab sample"
            );
        } else {
            sequence_num++;
            sample.sequence = sequence_num;
            if (wifi_manager_is_connected()) {
                result = replay_stored_telemetry();
                if (result == ESP_OK) {
                    result = replay_buffered_telemetry();
                }
                
                if (result == ESP_OK) {
                    result = send_telemetry_sample(&sample);
                }
                
                if (result != ESP_OK) {
                    ESP_LOGE(
                        TAG,
                        "Sample: %u failed to send",
                        (unsigned int) sample.sequence
                    );
                    telemetry_buffer_push(&sample);
                    
                } else {
                    ESP_LOGI(
                        TAG,
                        "Send succeeded for sample: %u",
                        (unsigned int)sample.sequence
                    );
                }
            } else {

                ESP_LOGI(
                    TAG,
                    "Wi-Fi disconnected, buffering sample: %u",
                    (unsigned int)sample.sequence
                );

                if (!telemetry_buffer_is_full()) {

                    telemetry_buffer_push(&sample);

                } else {

                    reading_t oldest;

                    if (telemetry_buffer_peek(&oldest)) {

                        if (nvs_store_pending_reading(&oldest) == ESP_OK) {

                            telemetry_buffer_pop();
                            telemetry_buffer_push(&sample);

                        } else {

                            ESP_LOGE(
                                TAG,
                                "Failed to spill buffered reading to NVS"
                            );
                        }
                    }
                }
            }
        }
        
        esp_task_wdt_reset();
            
        vTaskDelay(pdMS_TO_TICKS(SAMPLE_PERIOD_MS));
    }
}