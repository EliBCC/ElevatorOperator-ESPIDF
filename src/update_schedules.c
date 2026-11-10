#include <stdio.h>
#include <stdlib.h>

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "fetch_http.h"
#include "update_schedules.h"
#include "route_config.h"
#include "secrets.h"
#include "cJSON.h"

RealtimeStop *stop_singleton = NULL;
bool run_update_schedule_task = true;
SemaphoreHandle_t stop_singleton_mutex;
uint32_t schedule_generation = 0;

static const char *TAG = "update_schedules";

void init_schedule_updater()
{
    stop_singleton_mutex = xSemaphoreCreateMutex();
    xSemaphoreTake(stop_singleton_mutex, MAX_STOP_SINGETON_MUTEX_WAIT_MS);
    stop_singleton = (RealtimeStop *)calloc(1, sizeof(RealtimeStop));
    assert(stop_singleton != NULL);
    stop_singleton->departures = calloc(MAX_MERGED_DEPARTURES, sizeof(RealtimeDeparture));
    assert(stop_singleton->departures != NULL);
    xSemaphoreGive(stop_singleton_mutex);
}

void cleanup_schedule_updater()
{
    free(stop_singleton->departures);
    stop_singleton->departures = NULL;
    free(stop_singleton);
    stop_singleton = NULL;
}

void get_from_http(http_response_buf_t *response, size_t route_config_index, size_t route_number_index)
{
    ESP_LOGD(TAG, "In get_from_http");
    char url[100]; 
    GET_REALTIME_SCHEDULE_FOR_STOP_AND_ROUTE(url,
        ROUTE_CONFIGS[route_config_index].stop_number,
        ROUTE_CONFIGS[route_config_index].route_numbers[route_number_index],
        3);
    ESP_LOGD(TAG, "Requesting url: %s", url);

    ESP_LOGD(TAG, "Response has address %p", response);
    http_request_params_t request = {
        .response_buff = response,
        .url = url
    };

    http_get_task((void *)(&request));
}

void _parse_response_to_struct(http_response_buf_t *response, size_t route_config_index,
                               size_t route_number_index)
{
    if (response == NULL || response->buf == NULL || response->written == 0) {
        ESP_LOGW(TAG, "Cannot parse an empty response");
        return;
    }

    ESP_LOGD(TAG, "Parsing into JSON");
    cJSON *json_document = cJSON_ParseWithLength(response->buf, response->written);
    if (json_document == NULL) {
        ESP_LOGW(TAG, "Invalid JSON response");
        return;
    }

    cJSON *json = json_document;
    if (cJSON_IsArray(json_document)) {
        json = cJSON_GetArrayItem(json_document, 0);
    }
    if (!cJSON_IsObject(json)) {
        ESP_LOGW(TAG, "Response contains no schedule object");
        cJSON_Delete(json_document);
        return;
    }

    cJSON *searchDateUnix = cJSON_GetObjectItemCaseSensitive(json, "lu");
    cJSON *trips = cJSON_GetObjectItemCaseSensitive(json, "r");

    if (!cJSON_IsNumber(searchDateUnix) || !cJSON_IsArray(trips)) {
        ESP_LOGW(TAG, "Schedule response is missing required fields");
        cJSON_Delete(json_document);
        return;
    }

    RealtimeDeparture parsed_departures[MAX_SCHEDULES * MAX_STOP_TIMES_PER_SCHEDULE];
    size_t parsed_count = 0;
    uint32_t search_date_unix = (uint32_t)searchDateUnix->valuedouble;
    size_t schedule_count = 0;
    cJSON *j_trip;
    cJSON_ArrayForEach(j_trip, trips)
    {
        if (schedule_count++ >= MAX_SCHEDULES) {
            ESP_LOGW(TAG, "Too many schedules in response; truncating");
            break;
        }
        cJSON *j_times = cJSON_GetObjectItemCaseSensitive(j_trip, "t");
        if (!cJSON_IsArray(j_times)) {
            continue;
        }

        size_t time_count = 0;
        cJSON *j_time;
        cJSON_ArrayForEach(j_time, j_times)
        {
            if (time_count++ >= MAX_STOP_TIMES_PER_SCHEDULE) {
                ESP_LOGW(TAG, "Too many stop times in schedule; truncating");
                break;
            }
            cJSON *j_ut = cJSON_GetObjectItemCaseSensitive(j_time, "ut");
            if (!cJSON_IsNumber(j_ut)) {
                continue;
            }
            uint32_t departure_time = (uint32_t)j_ut->valuedouble;
            if (departure_time <= search_date_unix || parsed_count >= MAX_SCHEDULES * MAX_STOP_TIMES_PER_SCHEDULE) {
                continue;
            }
            parsed_departures[parsed_count++] = (RealtimeDeparture){
                .departure_time_unix = departure_time,
                .route_number = ROUTE_CONFIGS[route_config_index].route_numbers[route_number_index]};
        }
    }

    if (xSemaphoreTake(stop_singleton_mutex, pdMS_TO_TICKS(MAX_STOP_SINGETON_MUTEX_WAIT_MS)) != pdTRUE) {
        cJSON_Delete(json_document);
        return;
    }
    if (route_config_index == stop_singleton->route_config_index) {
        if (search_date_unix > stop_singleton->search_date_unix) {
            stop_singleton->search_date_unix = search_date_unix;
        }
        for (size_t i = 0; i < parsed_count; i++) {
            size_t insert_at = stop_singleton->departure_count;
            if (insert_at >= MAX_MERGED_DEPARTURES) {
                ESP_LOGW(TAG, "Merged departure buffer full; truncating");
                break;
            }
            while (insert_at > 0 && stop_singleton->departures[insert_at - 1].departure_time_unix >
                    parsed_departures[i].departure_time_unix) {
                stop_singleton->departures[insert_at] = stop_singleton->departures[insert_at - 1];
                insert_at--;
            }
            stop_singleton->departures[insert_at] = parsed_departures[i];
            stop_singleton->departure_count++;
        }
    }
    xSemaphoreGive(stop_singleton_mutex);
    cJSON_Delete(json_document);
}

void _update_schedule(size_t route_config_index, size_t route_number_index)
{
    ESP_LOGD(TAG, "In update_schedule");
    char raw_response_buffer[MAX_HTTP_OUTPUT_BUFFER] = {0};
    http_response_buf_t response = {
        .buf = raw_response_buffer,
        .size = MAX_HTTP_OUTPUT_BUFFER,
        .written = 0
    };

    get_from_http(&response, route_config_index, route_number_index);

    ESP_LOGI(TAG, "Received: \n%.*s", response.written, response.buf);

    if(response.written == 0)
    {
        ESP_LOGW(TAG, "Did not receive data from HTTP");
        return;
    }

    _parse_response_to_struct(&response, route_config_index, route_number_index);
}

static void begin_route_group(size_t route_config_index)
{
    if (xSemaphoreTake(stop_singleton_mutex, pdMS_TO_TICKS(MAX_STOP_SINGETON_MUTEX_WAIT_MS)) == pdTRUE) {
        stop_singleton->route_config_index = route_config_index;
        stop_singleton->search_date_unix = 0;
        stop_singleton->departure_count = 0;
        xSemaphoreGive(stop_singleton_mutex);
    }
}

void update_schedule()
{
    size_t request_count = 0;
    for (size_t i = 0; i < ROUTE_CONFIG_COUNT; i++) {
        request_count += ROUTE_CONFIGS[i].route_count;
    }
    if (request_count == 0) {
        ESP_LOGE(TAG, "No route requests are configured");
        return;
    }

    TickType_t route_period = PERIOD_MS / request_count;
    if (route_period == 0) {
        route_period = 1;
    }

    while(run_update_schedule_task)
    {
        for (size_t route_config_index = 0;
                route_config_index < ROUTE_CONFIG_COUNT && run_update_schedule_task;
                route_config_index++) {
            begin_route_group(route_config_index);
            for (size_t route_number_index = 0;
                    route_number_index < ROUTE_CONFIGS[route_config_index].route_count;
                    route_number_index++) {
                _update_schedule(route_config_index, route_number_index);
                if (route_number_index + 1 < ROUTE_CONFIGS[route_config_index].route_count) {
                    vTaskDelay(route_period);
                }
            }

            if (xSemaphoreTake(stop_singleton_mutex, pdMS_TO_TICKS(MAX_STOP_SINGETON_MUTEX_WAIT_MS)) == pdTRUE) {
                schedule_generation++;
                xSemaphoreGive(stop_singleton_mutex);
            }
            vTaskDelay(route_period);
        }
    }
}
