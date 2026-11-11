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
#include "update_schedules.h"
#include "display.h"

static const char *TAG = "main";

void loop(void)
{
    while(true)
    {
        vTaskDelay(portMAX_DELAY);
    }
}

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
    init_schedule_updater();
    display();
    BaseType_t task_created = xTaskCreate(&update_schedule, "update_schedule", 16384, NULL, 5, NULL);
    ESP_ERROR_CHECK(task_created == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);

    loop();

    cleanup_schedule_updater();

}

