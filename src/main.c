#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include "esp_log.h"

#include "nvs_flash.h"

#include "main.h"
#include "secrets.h"
#include "fetch_http.h"

#define BAUD_RATE           11520

static const char *TAG = "main";


void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    esp_log_level_set("*", ESP_LOG_DEBUG);

    wifi_init_sta();

    xTaskCreate(&http_get_task, "http_get_task", 16384, NULL, 5, NULL);
}

