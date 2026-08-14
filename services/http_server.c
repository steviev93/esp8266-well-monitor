
#include "esp_http_server.h"
#include "app_types.h"
#include "nvs_service.h"

#define SSID_SIZE                    33
#define PASSWORD_SIZE                65
#define ITEM_SIZE                    384
#define MAX_REQUEST_BODY_SIZE        256

static httpd_handle_t g_http_server = NULL;

static esp_err_t handle_root(httpd_req_t *req)
{
    const char *html =
        "<html><body>"
        "<h1>Well Monitor</h1>"
        "<p>ESP is awake continuously. Sensor loop is only powered during sampling.</p>"
        "<ul>"
        "<li>GET /reading</li>"
        "<li>GET /readings</li>"
        "<li>POST /sample</li>"
        "<li>GET /config</li>"
        "<li>POST /config</li>"
        "<li>POST /wifi</li>"
        "</ul>"
        "</body></html>";

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html, strlen(html));
    return ESP_OK;
}

static esp_err_t handle_get_reading(httpd_req_t *req)
{
    reading_t r;
    if (!get_latest_reading(&r)) {
        send_json(req, "{\"ok\":false,\"error\":\"no_reading_yet\"}");
        return ESP_OK;
    }

    char json[ITEM_SIZE];
    snprintf(
        json,
        sizeof(json),
        "{"
        "\"ok\":true,"
        "\"sequence\":%u,"
        "\"uptime_ms\":%u,"
        "\"ads_raw\":%d,"
        "\"shunt_uV\":%ld,"
        "\"shunt_mV\":%ld,"
        "\"loop_uA\":%ld,"
        "\"normalized_permille\":%ld,"
        "\"level_percent_x100\":%ld,"
        "\"water_level_mm\":%ld"
        "}",
        r.sequence,
        r.uptime_ms,
        r.ads_raw,
        (long)r.shunt_uV,
        (long)r.shunt_mV,
        (long)r.loop_uA,
        (long)r.normalized_permille,
        (long)r.level_percent_x100,
        (long)r.water_level_mm
    );

    send_json(req, json);
    return ESP_OK;
}


//TODO: limit number of readings in chunks. Don't try to load all of them
static esp_err_t handle_get_readings(httpd_req_t *req)
{
    reading_store_t reading_store = {0};
    esp_err_t result = get_reading_store(&reading_store);

    if (result != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Error reading store: %s",
            esp_err_to_name(result)
        );
        send_json(
            req,
            "{\"ok\":false,\"error\":\"reading_store_failed\"}"
        );

        return ESP_OK;
    }

    char json[ITEM_SIZE*50];

    uint32_t count = reading_store.count;

    size_t offset = 0;

    offset += snprintf(
        json,
        sizeof(json),
        "{\"ok\":true,\"count\":%u,\"readings\":[",
        count
    );

    for (uint32_t i = 0; i < count; i++) {
        uint32_t idx = (reading_store.head + MAX_READINGS - count + i) % MAX_READINGS;
        reading_t *r = &reading_store.readings[idx];

        offset += snprintf(
            json + offset,
            sizeof(json) - offset,
            "%s{"
            "\"sequence\":%u,"
            "\"uptime_ms\":%u,"
            "\"ads_raw\":%d,"
            "\"shunt_uV\":%ld,"
            "\"shunt_mV\":%ld,"
            "\"loop_uA\":%ld,"
            "\"normalized_permille\":%ld,"
            "\"level_percent_x100\":%ld,"
            "\"water_level_mm\":%ld"
            "}",
            i == 0 ? "" : ",",
            r->sequence,
            r->uptime_ms,
            r->ads_raw,
            (long)r->shunt_uV,
            (long)r->shunt_mV,
            (long)r->loop_uA,
            (long)r->normalized_permille,
            (long)r->level_percent_x100,
            (long)r->water_level_mm
        );
    }

    offset += snprintf(
        json + offset,
        sizeof(json) - offset,
        "]}"
    );

    send_json(req, json);
    return ESP_OK;
}

// why is this even here? 
// We should be taking incremental samples and serving them, not posting them
// ok, maybe we can have a task in board that incrementally takes, then use this like an int?
static esp_err_t handle_post_sample(httpd_req_t *req)
{
    reading_t r;
    if (!take_sensor_reading(&r)) {
        send_json(req, "{\"ok\":false,\"error\":\"sample_failed\"}");
        return ESP_OK;
    }

    add_reading(&r);

    char json[384];
    snprintf(
        json,
        sizeof(json),
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
        r->sequence,
        r->ads_raw,
        (long)r->shunt_uV,
        (long)r->shunt_mV,
        (long)r->loop_uA,
        (long)r->normalized_permille,
        (long)r->level_percent_x100,
        (long)r->water_level_mm
    );

    send_json(req, json);
    return ESP_OK;
}

static esp_err_t handle_get_config(httpd_req_t *req)
{
    app_config_t config;
    bool result = get_latest_config(&config);

    if (!result) {
        ESP_LOGE(
            TAG,
            "Failed to get config"
        );
        send_json(req, "{\"ok\":false,\"error\":\"sample_failed\"}");
        return ESP_OK;
    }
    char json[256];
    snprintf(
        json,
        sizeof(json),
        "{"
        "\"ok\":true,"
        "\"sample_settle_ms\":%u,"
        "\"retention_count\":%u,"
        "\"max_readings\":%u"
        "}",
        config.sample_settle_ms,
        config.retention_count,
        MAX_READINGS
    );

    send_json(req, json);
    return ESP_OK;
}

static esp_err_t handle_post_config(httpd_req_t *req)
{
    
    char *body = read_request_body(req);
    uint32_t value;

    app_config_t config;

    if (json_find_uint(
            body,
            "sample_settle_ms",
            &value
        )
    ) {
        config.sample_settle_ms = value;
    }

    if (json_find_uint(body, "retention_count", &value)) {
        config.retention_count = value;
    }

    save_config(&config);

    send_json(req, "{\"ok\":true}");
    free(body);
    return ESP_OK;
}

static esp_err_t handle_post_wifi(httpd_req_t *req)
{
    char *body = read_request_body(req);

    char ssid[SSID_SIZE] = {0};
    char password[PASSWORD_SIZE] = {0};

    if (!json_find_string(body, "ssid", ssid, sizeof(ssid))) {
        send_json(req, "{\"ok\":false,\"error\":\"missing_ssid\"}");
        free(body);
        return ESP_OK;
    }

    if (!json_find_string(body, "password", password, sizeof(password))) {
        password[0] = '\0';
    }

    esp_err_t result = save_wifi_credentials(ssid, password);
    free(body);

    if (result != ESP_OK) {
        char json[256];
        snprintf(
            json,
            sizeof(json),
            "{\"ok\":false,\"message\":\"SAVE FAILED!! %s\"}",
            esp_err_to_name(result)
        );
        send_json(req, json);
        return result;
    }
    send_json(req, "{\"ok\":true,\"message\":\"wifi_saved_rebooting\"}");

    reboot_soon();
    return ESP_OK;
}

void start_http_server()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_uri_handlers = 10;

    ESP_ERROR_CHECK(httpd_start(&g_http_server, &config));

    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = handle_root,
        .user_ctx = NULL
    };

    httpd_uri_t get_reading_uri = {
        .uri = "/reading",
        .method = HTTP_GET,
        .handler = handle_get_reading,
        .user_ctx = NULL
    };

    httpd_uri_t get_readings_uri = {
        .uri = "/readings",
        .method = HTTP_GET,
        .handler = handle_get_readings,
        .user_ctx = NULL
    };

    httpd_uri_t post_sample_uri = {
        .uri = "/sample",
        .method = HTTP_POST,
        .handler = handle_post_sample,
        .user_ctx = NULL
    };

    httpd_uri_t get_config_uri = {
        .uri = "/config",
        .method = HTTP_GET,
        .handler = handle_get_config,
        .user_ctx = NULL
    };

    httpd_uri_t post_config_uri = {
        .uri = "/config",
        .method = HTTP_POST,
        .handler = handle_post_config,
        .user_ctx = NULL
    };

    httpd_uri_t post_wifi_uri = {
        .uri = "/wifi",
        .method = HTTP_POST,
        .handler = handle_post_wifi,
        .user_ctx = NULL
    };

    httpd_register_uri_handler(g_http_server, &root_uri);
    httpd_register_uri_handler(g_http_server, &get_reading_uri);
    httpd_register_uri_handler(g_http_server, &get_readings_uri);
    httpd_register_uri_handler(g_http_server, &post_sample_uri);
    httpd_register_uri_handler(g_http_server, &get_config_uri);
    httpd_register_uri_handler(g_http_server, &post_config_uri);
    httpd_register_uri_handler(g_http_server, &post_wifi_uri);

    ESP_LOGI(TAG, "HTTP server started");
}


static char * read_request_body(httpd_req_t *req)
{
    if (req == NULL || req->content_len == 0 || req->content_len > MAX_REQUEST_BODY_SIZE) {
        return NULL;
    }
    
    char *body = malloc(req->content_len + 1);

    if (body == NULL) {
        return NULL;
    }

    size_t received = 0;

    while (received < req->content_len) {
        int ret = httpd_req_recv(
            req,
            body + received,
            req->content_len - received
        );

        if (ret <= 0) {
            free(body);
            return NULL;
        }

        received += ret;
    }

    body[received] = '\0';
    
    return body;
}

static bool json_find_uint(const char *body, const char *key, uint32_t *out)
{

    if (body == NULL || key == NULL || out == NULL) {
        return false;
    }

    char pattern[64];

    int written = snprintf(
        pattern,
        sizeof(pattern),
        "\"%s\"",
        key
    );

    if (written < 0 || written >= sizeof(pattern)) {
        return false;
    }

    char *pos = strstr(body, pattern);

    if (pos == NULL) {
        return false;
    }

    pos = strchr(pos, ':');

    if (pos == NULL) {
        return false;
    }

    pos++;

    while (*pos == ' ' || *pos == '\t') {
        pos++;
    }

    char *end = NULL;

    unsigned long value = strtoul(
        pos,
        &end,
        10
    );

    if (end == pos || ) {
        return false;
    }

    *out = (uint32_t)value;
    return true;
}

static bool json_find_string(
    const char *body,
    const char *key,
    char *out,
    size_t out_size)
{
    if (body == NULL ||
        key == NULL ||
        out == NULL ||
        out_size == 0) {
        return false;
    }

    char pattern[64];

    int written = snprintf(
        pattern,
        sizeof(pattern),
        "\"%s\"",
        key
    );

    if (written < 0 ||
        (size_t)written >= sizeof(pattern)) {
        return false;
    }

    char *pos = strstr(body, pattern);

    if (pos == NULL) {
        return false;
    }

    pos = strchr(pos, ':');

    if (pos == NULL) {
        return false;
    }

    pos++;

    while (*pos == ' ' || *pos == '\t') {
        pos++;
    }

    if (*pos != '"') {
        return false;
    }

    pos++;

    char *end = strchr(pos, '"');

    if (end == NULL) {
        return false;
    }

    size_t len = end - pos;

    if (len >= out_size) {
        return false;
    }

    memcpy(out, pos, len);
    out[len] = '\0';

    return true;
}

static esp_err_t send_json(httpd_req_t *req, const char *json)
{
    if (req == NULL || json == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t err = httpd_resp_set_type(req, "application/json");

    if (err != ESP_OK) {
        return err;
    }

    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}