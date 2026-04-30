#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_crt_bundle.h"
#include "esp_event.h"

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"

#include "nvs_flash.h"

#include "esp_http_client.h"
#include "esp_netif.h" // Required for esp_netif_init

#include "main.h"
#include "secrets.h"

#define MAX_FAILURES   10

#define MAX_HTTP_OUTPUT_BUFFER 2048

static const char *TAG = "http_get_example";

static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
static int s_retry_num = 0;

// Event handler for Wi-Fi
static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < MAX_FAILURES) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// Initialize Wi-Fi
void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init()); // Initialize TCP/IP stack

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s", WIFI_SSID);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s", WIFI_SSID);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}

// HTTP event handler
esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    http_response_buf_t *resp = (http_response_buf_t *)evt->user_data;
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            // // If user_data is not NULL, it means buffer is passed from user, append data to it.
            // if (evt->user_data != NULL) {
            //     ESP_LOGD(TAG, "user_data is not NULL, appending");
            //     memcpy(evt->user_data + output_len, evt->data, evt->data_len);
            // } else { // Otherwise, allocate new buffer and copy data.
            //     ESP_LOGD(TAG, "user_data is NULL, create new buffer");
            //     if (output_buffer == NULL) {
            //         size_t output_buffer_size = (size_t) esp_http_client_get_content_length(evt->client) + 1;
            //         ESP_LOGD(TAG, "output_buffer is null, allocate %d bytes", output_buffer_size);
            //         ESP_LOGD(TAG, "heap_caps_get_largest_free_block: %d\n", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
            //         // Allocate dynamic buffer to store response data
            //         output_buffer = (char *) malloc(output_buffer_size);
            //         output_len = 0;
            //         if (output_buffer == NULL) {
            //             ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
            //             return ESP_FAIL;
            //         }
            //     }
            if (resp->written + evt->data_len >= resp->size) {
                ESP_LOGE(TAG, "Response too large for buffer. Writing %d bytes, found %d bytes remaining.", evt->data_len, resp->size - resp->written);
                return ESP_FAIL;
            }
            memcpy(resp->buf + resp->written, evt->data, evt->data_len);
            resp->written += evt->data_len;
            ESP_LOGD(TAG, "buffer contains %d bytes", resp->written);
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            ESP_LOGD(TAG, "Received: \n%.*s", resp->written, resp->buf);
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            // int mbedtls_err = 0;
            // esp_err_t err = esp_tls_get_and_clear_last_error(evt->data, &mbedtls_err, NULL);
            // if (err != 0) {
            //     ESP_LOGI(TAG, "Last mbedtls error: 0x%x", mbedtls_err);
            // }
            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
            esp_http_client_set_redirection(evt->client);
            break;
    }
    return ESP_OK;
}

static void http_get_task(void *pvParameters)
{
    // Wait for Wi-Fi connection
    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
    ESP_LOGI(TAG, "Connected to Wi-Fi, starting HTTP GET task.");

    char local_response_buffer[MAX_HTTP_OUTPUT_BUFFER] = {0};
    http_response_buf_t response = {
        .buf = local_response_buffer,
        .size = sizeof(local_response_buffer),
        .written = 0
    };

    esp_http_client_config_t config = {
        // .url = "http://wifitest.adafruit.com/testwifi/index.html",
        .url = "https://getaway.translink.ca/api/gtfs/stop/50916/route/9/realtimeschedules?querySize=1",
        .event_handler = _http_event_handler,
        .user_data = &response, // Pass address of local buffer to event handler
        .buffer_size = sizeof(local_response_buffer),         // Let client know how much data it can put in the buffer
        .disable_auto_redirect = false,     // Handle redirects automatically
        .transport_type = HTTP_TRANSPORT_OVER_SSL,  //Specify transport type
        .crt_bundle_attach = esp_crt_bundle_attach, //Attach the certificate bundle 
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    // Perform the HTTP request
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %lld",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
        // The response body is printed by the event handler (HTTP_EVENT_ON_FINISH)
        // using the local_response_buffer.
    } else {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    vTaskDelete(NULL);
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    esp_log_level_set("*", ESP_LOG_VERBOSE);

    wifi_init_sta();

    xTaskCreate(&http_get_task, "http_get_task", 16384, NULL, 5, NULL);
}

