#pragma once

#include "esp_http_server.h"
#include "app_types.h"
#include "nvs_service.h"

#define SSID_SIZE                    33
#define PASSWORD_SIZE                65
#define ITEM_SIZE                    384
#define MAX_REQUEST_BODY_SIZE        256

static esp_err_t handle_root(httpd_req_t *req);

static esp_err_t handle_get_reading(httpd_req_t *req);

static esp_err_t handle_post_sample(httpd_req_t *req);

static esp_err_t handle_get_config(httpd_req_t *req);

static esp_err_t handle_post_config(httpd_req_t *req);

static esp_err_t handle_post_wifi(httpd_req_t *req);

void start_http_server();

static char * read_request_body(httpd_req_t *req);

static bool json_find_uint(const char *body, const char *key, uint32_t *out);

static bool json_find_string(
    const char *body,
    const char *key,
    char *out,
    size_t out_size);

static esp_err_t send_json(httpd_req_t *req, const char *json);