#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "http_server.h"
#include "wifi_manager.h"

#include "nvs_service.h"
#include "tcpip_adapter.h"


#define WIFI_MAXIMUM_RETRY_COUNT   5U

#define WIFI_CONNECTED_BIT         BIT0
#define WIFI_FAILED_BIT            BIT1
#define SSID_SIZE                  33
#define PASSWORD_SIZE              65


#define WIFI_MONITOR_PERIOD_MS     2000U

static const char *TAG = "wifi_manager";

typedef enum {
    WIFI_MANAGER_STATE_UNINITIALIZED = 0,
    WIFI_MANAGER_STATE_DISCONNECTED,
    WIFI_MANAGER_STATE_CONNECTING,
    WIFI_MANAGER_STATE_CONNECTED,
    WIFI_MANAGER_STATE_FAILED
} wifi_manager_state_t;

static EventGroupHandle_t wifi_event_group = NULL;

static wifi_manager_state_t wifi_manager_state =
    WIFI_MANAGER_STATE_UNINITIALIZED;

wifi_config_t station_configuration;

static uint32_t wifi_retry_count = 0U;

char ssid[SSID_SIZE];
char password[PASSWORD_SIZE];

static void configure_static_ip()
{
    tcpip_adapter_ip_info_t ip_info;
    memset(&ip_info, 0, sizeof(ip_info));

    IP4_ADDR(&ip_info.ip, 192, 168, 68, 156);       // ESP fixed IP
    IP4_ADDR(&ip_info.gw, 192, 168, 68, 1);        // router/gateway
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0); // subnet mask

    ESP_ERROR_CHECK(tcpip_adapter_dhcpc_stop(TCPIP_ADAPTER_IF_STA));
    ESP_ERROR_CHECK(tcpip_adapter_set_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info));

    tcpip_adapter_dns_info_t dns;
    memset(&dns, 0, sizeof(dns));

    IP4_ADDR(&dns.ip, 192, 168, 68, 1); // usually your router
    tcpip_adapter_set_dns_info(TCPIP_ADAPTER_IF_STA, TCPIP_ADAPTER_DNS_MAIN, &dns);

    ESP_LOGI(TAG, "Static IP configured: 192.168.68.55");
}

static void wifi_event_handler(
    void *handler_argument,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data)
{

    (void)handler_argument;
    esp_err_t result;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        wifi_manager_state = WIFI_MANAGER_STATE_CONNECTING;
        result = esp_wifi_connect();
        if (result != ESP_OK) {
            ESP_LOGE(
                TAG,
                "Connection request couldn't be started"
            );
        }
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_manager_state = WIFI_MANAGER_STATE_DISCONNECTED;
        xEventGroupClearBits(
            wifi_event_group,
            WIFI_CONNECTED_BIT
        );
        if (wifi_retry_count < WIFI_MAXIMUM_RETRY_COUNT) {
            wifi_retry_count++;
            wifi_manager_state = WIFI_MANAGER_STATE_CONNECTING;
            ESP_LOGW(
                TAG,
                "Retry number: %u",
                (unsigned int)wifi_retry_count
            );
            result = esp_wifi_connect();
            if (result != ESP_OK) {
                ESP_LOGE(
                    TAG,
                    "Retry failed: %s",
                    esp_err_to_name(result)
                );
            }
        } else {
            wifi_manager_state = WIFI_MANAGER_STATE_FAILED;
            xEventGroupSetBits(wifi_event_group, WIFI_FAILED_BIT);
            ESP_LOGW(
                TAG,
                "retry limit reached!"
            );
        }
    }


    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *got_ip = 
            (ip_event_got_ip_t *)event_data;
        
        wifi_retry_count = 0;
        wifi_manager_state = WIFI_MANAGER_STATE_CONNECTED;
        xEventGroupClearBits(wifi_event_group, WIFI_FAILED_BIT);
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGI(
            TAG,
            "IP address: " IPSTR,
            IP2STR(&got_ip->ip_info.ip)
        );

    }
}

static esp_err_t wifi_init_common() {
    esp_err_t error;
    wifi_init_config_t wifi_initialization_configuration =
        WIFI_INIT_CONFIG_DEFAULT();

    wifi_event_group = xEventGroupCreate();

    if (wifi_event_group == NULL) {
        ESP_LOGE(
            TAG,
            "event group creation failed"
        );
        return ESP_ERR_NO_MEM;
    }

    tcpip_adapter_init();

    error = esp_event_loop_create_default();

    if (error != ESP_OK) {
        ESP_LOGE(
            TAG,
            "error creating event loop: %s",
            esp_err_to_name(error)
        );
        return error;
    }

    error = esp_wifi_init(&wifi_initialization_configuration);
    if (error != ESP_OK) {
        ESP_LOGE(
            TAG,
            "wifi_initialization failed: %s",
            esp_err_to_name(error)
        );
        return error;
    }

    error = esp_event_handler_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        wifi_event_handler,
        NULL
    );
    if (error != ESP_OK) {
        ESP_LOGE(
            TAG,
            "wifi_event_handler failed: %s",
            esp_err_to_name(error)
        );
        return error;
    }

     error = esp_event_handler_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        wifi_event_handler,
        NULL
    );
    if (error != ESP_OK) {
        ESP_LOGE(
            TAG,
            "IP_EVENT event handler registration failed: %s",
            esp_err_to_name(error)
        );
        
    }
    return error;
}

static esp_err_t start_setup_ap_mode()
{
    esp_err_t result;
    wifi_config_t ap_config;
    memset(&ap_config, 0, sizeof(ap_config));

    uint8_t mac[6] = {0};
    esp_wifi_get_mac(ESP_IF_WIFI_STA, mac);

    char ap_ssid[32];
    snprintf(ap_ssid, sizeof(ap_ssid), "WellMonitor-%02X%02X", mac[4], mac[5]);

    strncpy((char *)ap_config.ap.ssid, ap_ssid, sizeof(ap_config.ap.ssid) - 1);
    ap_config.ap.ssid_len = strlen(ap_ssid);
    ap_config.ap.channel = 6;
    ap_config.ap.authmode = WIFI_AUTH_OPEN;
    ap_config.ap.max_connection = 2;

    result = esp_wifi_set_mode(WIFI_MODE_AP);
    if (result != ESP_OK) {
        return result;
    }
    result = esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_config);
    if (result != ESP_OK) {
        return result;
    }
    result = esp_wifi_start();

    ESP_LOGI(TAG, "Setup AP mode started: %s", ap_ssid);

    return result;
}

static esp_err_t start_station_mode(const char *ssid, const char *password)
{
    esp_err_t error;
    memset(&station_configuration, 0, sizeof(station_configuration));

     strncpy(
        (char *)station_configuration.sta.ssid, 
        ssid,
        sizeof(station_configuration.sta.ssid)
    );
    strncpy(
        (char *)station_configuration.sta.password, 
        password,
        sizeof(station_configuration.sta.password)
    );

    error = esp_wifi_set_mode(WIFI_MODE_STA);
    if (error != ESP_OK) {
        ESP_LOGE(
            TAG,
            "station mode configuration failed: %s",
            esp_err_to_name(error)
        );
        return error;
    }

    error = esp_wifi_set_config(
        ESP_IF_WIFI_STA,
        &station_configuration
    );
    if (error != ESP_OK) {
        ESP_LOGE(
            TAG,
            "wifi configuration failed: %s",
            esp_err_to_name(error)
        );
        return error;
    }

    wifi_retry_count = 0U;
    wifi_manager_state = WIFI_MANAGER_STATE_DISCONNECTED;
    xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAILED_BIT);

    configure_static_ip();

    error = esp_wifi_start();
    if (error != ESP_OK) {
        ESP_LOGE(
            TAG,
            "wifi start failed: %s",
            esp_err_to_name(error)
        );
        return error;
    }
    return ESP_OK;
}

esp_err_t wifi_manager_initialize(void)
{
    esp_err_t result;
    bool setup_mode = false;

    result = wifi_init_common();

    if (result != ESP_OK) {
        return result;
    }

    // credentials
    bool have_wifi = load_wifi_credentials(ssid, password, SSID_SIZE, PASSWORD_SIZE);

    if (have_wifi) {
        result = start_station_mode(ssid, password);

        if (result != ESP_OK) {
            ESP_LOGE(
                TAG,
                "Station failed to init: %s",
                esp_err_to_name(result)
            );
            return result;
        }

        EventBits_t bits = xEventGroupWaitBits(
            wifi_event_group,
            WIFI_CONNECTED_BIT,
            false,
            true,
            pdMS_TO_TICKS(20000)
        );

        if (!(bits & WIFI_CONNECTED_BIT)) {
            ESP_LOGW(TAG, "Wi-Fi connection failed; starting setup AP");

            esp_wifi_stop();

            result = start_setup_ap_mode();

            if (result != ESP_OK) {
                return result;
            }

            setup_mode = true;
        }
    } else {
        ESP_LOGW(TAG, "No Wi-Fi credentials; starting setup AP");
        result = start_setup_ap_mode();

        if (result != ESP_OK) {
            return result;
        }

        setup_mode = true;
    }

    start_http_server(setup_mode);

    return ESP_OK;
}

bool wifi_manager_is_connected(void) {
    EventBits_t event_bits = xEventGroupGetBits(wifi_event_group);
    return (event_bits & WIFI_CONNECTED_BIT) != 0U;
}