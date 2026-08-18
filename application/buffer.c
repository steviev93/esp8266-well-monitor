#include "buffer.h"

static reading_buffer_t telemetry_buffer;

static const char *TAG = "buffer";

void telemetry_buffer_initialize(void)
{
    memset(&telemetry_buffer, 0, sizeof(telemetry_buffer));
    telemetry_buffer.head = 0;
    telemetry_buffer.tail = 0;
    telemetry_buffer.count = 0;

}

bool telemetry_buffer_is_empty(void)
{
    return telemetry_buffer.count == 0;
}

bool telemetry_buffer_is_full(void)
{
    return telemetry_buffer.count == TELEMETRY_BUFFER_CAPACITY;
}

void telemetry_buffer_push(const reading_t *sample)
{
    if (telemetry_buffer_is_full()) {
        ESP_LOGI(
            TAG,
            "Push to buffer for sample: %u failed",
            (unsigned int)sample->sequence
        );
        return;
    }
    telemetry_buffer.readings[telemetry_buffer.head] = *sample;
    telemetry_buffer.head++;
    
    if (telemetry_buffer.head == TELEMETRY_BUFFER_CAPACITY) {
        telemetry_buffer.head = 0;
    } 

    telemetry_buffer.count++;
    ESP_LOGI(
        TAG,
        "Push to buffer for sample: %u succeeded",
        (unsigned int)sample->sequence
    );
    return;
}

bool telemetry_buffer_peek(reading_t *sample_out)
{

    if (sample_out == NULL || telemetry_buffer_is_empty()){
        return false;
    }
    
    *sample_out = telemetry_buffer.readings[telemetry_buffer.tail];
    
    return true;
}

bool telemetry_buffer_pop(void)
{
    if (telemetry_buffer_is_empty()) {
        return false;
    }

    telemetry_buffer.tail++;
    if (telemetry_buffer.tail == TELEMETRY_BUFFER_CAPACITY) {
        telemetry_buffer.tail = 0;
    }
    telemetry_buffer.count--;
    return true;
}

size_t telemetry_buffer_count(void)
{
    return telemetry_buffer.count;
}