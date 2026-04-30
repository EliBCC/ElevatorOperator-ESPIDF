#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "fetch_http.h"
#include "secrets.h"
#include "translink.h"
#include "cJSON.h"

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

static const char *TAG = "update_schedules";

void get_from_http(http_response_buf_t *response)
{
    ESP_LOGD(TAG, "In get_from_http");
    // "https://getaway.translink.ca/api/gtfs/stop/50916/route/9/realtimeschedules?querySize=1"
    char url[100]; 
    GET_REALTIME_SCHEDULE_FOR_STOP_AND_ROUTE(url, 50916, 9, 1);
    ESP_LOGD(TAG, "Requesting url: %s", url);

    ESP_LOGD(TAG, "Response has address %p", response);
    http_request_params_t request = {
        .response_buff = response,
        .url = url
    };

    xTaskCreate(&http_get_task, "http_get_task", 16384, (void *)(&request), 5, NULL);
}

void update_schedule()
{
    ESP_LOGD(TAG, "In update_schedule");
    char raw_response_buffer[MAX_HTTP_OUTPUT_BUFFER] = {0};
    http_response_buf_t response = {
        .buf = raw_response_buffer,
        .size = MAX_HTTP_OUTPUT_BUFFER,
        .written = 0
    };

    get_from_http(&response);

    ESP_LOGI(TAG, "Received: \n%.*s", response.written, response.buf);

    if(response.written == 0)
    {
        ESP_LOGW(TAG, "Did not receive data from HTTP");
        return;
    }

}