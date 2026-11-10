#include <stdio.h>
#include <stdlib.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "fetch_http.h"
#include "update_schedules.h"
#include "route_config.h"
#include "secrets.h"
#include "cJSON.h"

RealtimeStop *stop_singleton = NULL;
static cJSON *json_singleton = NULL;
bool run_update_schedule_task = true;
SemaphoreHandle_t stop_singleton_mutex;
uint32_t schedule_generation = 0;

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

static const char *TAG = "update_schedules";

void init_schedule_updater()
{
    stop_singleton_mutex = xSemaphoreCreateMutex();
    xSemaphoreTake(stop_singleton_mutex, MAX_STOP_SINGETON_MUTEX_WAIT_MS);
    stop_singleton = (RealtimeStop *)calloc(1, sizeof(RealtimeStop));
    assert(stop_singleton != NULL);
    RealtimeSchedule *schedules = (RealtimeSchedule *)calloc(1, MAX_SCHEDULES * sizeof(RealtimeSchedule));
    assert(schedules != NULL);
    ESP_LOGD(TAG, "Creating schedules object with pointer address %p", schedules);
    stop_singleton->trips = schedules;
    ESP_LOGD(TAG, "Assigend value %p to trips", stop_singleton->trips);
    for (int i = 0; i < MAX_SCHEDULES; i++)
    {
        RealtimeStopTime *stop_times = (RealtimeStopTime *)calloc(1, MAX_STOP_TIMES_PER_SCHEDULE * sizeof(RealtimeStopTime));
        assert(stop_times != NULL);
        stop_singleton->trips[i].stopTimes = stop_times;
    }
    xSemaphoreGive(stop_singleton_mutex);
}

void cleanup_schedule_updater()
{
    for(int i = 0; i < MAX_SCHEDULES; i++)
    {
        free(stop_singleton->trips[i].stopTimes);
        stop_singleton->trips[i].stopTimes = NULL;
    }
    free(stop_singleton->trips);
    stop_singleton->trips = NULL;
    free(stop_singleton);
    stop_singleton = NULL;
    cJSON_Delete(json_singleton);
    json_singleton = NULL;
}

void get_from_http(http_response_buf_t *response)
{
    ESP_LOGD(TAG, "In get_from_http");
    char url[100]; 
        GET_REALTIME_SCHEDULE_FOR_STOP_AND_ROUTE(url,
            ROUTE_CONFIGS[0].stop_number,
            ROUTE_CONFIGS[0].route_number,
            3);
    ESP_LOGD(TAG, "Requesting url: %s", url);

    ESP_LOGD(TAG, "Response has address %p", response);
    http_request_params_t request = {
        .response_buff = response,
        .url = url
    };

    http_get_task((void *)(&request));
}

void _parse_response_to_struct(http_response_buf_t *response)
{
    // Strip square brackets, if present
    int lhs_offset = 0;
    int rhs_offset = 0;
    if (response->buf[0] == '[')
    {
        lhs_offset = 1;
    }
    if (response->buf[response->written - 1] == ']')
    {
        rhs_offset = 1;
    }

    ESP_LOGD(TAG, "Parsing into JSON");

    // Parse into JSON
    cJSON *json = cJSON_ParseWithLength(response->buf + 1, response->written - lhs_offset - rhs_offset);

    cJSON *id = cJSON_GetObjectItemCaseSensitive(json, "id");
    cJSON *stopID = cJSON_GetObjectItemCaseSensitive(json, "sc");
    cJSON *stopName = cJSON_GetObjectItemCaseSensitive(json, "sn");
    cJSON *latitude = cJSON_GetObjectItemCaseSensitive(json, "la");
    cJSON *longitude = cJSON_GetObjectItemCaseSensitive(json, "lo");
    cJSON *wc = cJSON_GetObjectItemCaseSensitive(json, "wc");
    cJSON *searchDate = cJSON_GetObjectItemCaseSensitive(json, "sd");
    cJSON *searchDateUnix = cJSON_GetObjectItemCaseSensitive(json, "lu");
    cJSON *trips = cJSON_GetObjectItemCaseSensitive(json, "r");

    ESP_LOGD(TAG, "Parsing into stop singleton struct with pointer %p", stop_singleton);
    ESP_LOGD(TAG, "Before, trips has pointer %p", stop_singleton->trips);
    xSemaphoreTake(stop_singleton_mutex, MAX_STOP_SINGETON_MUTEX_WAIT_MS);
    *stop_singleton = (RealtimeStop){
        .id = cJSON_GetStringValue(id), // don't use cJSON_Print
        .stopID = cJSON_GetStringValue(stopID),
        .stopName = cJSON_GetStringValue(stopName),
        .latitude = (float)latitude->valuedouble, // direct field access
        .longitude = (float)longitude->valuedouble,
        .wc = wc->valueint,
        .searchDate = cJSON_GetStringValue(searchDate),
        .searchDateUnix = (unsigned int)searchDateUnix->valuedouble,
        .trips = stop_singleton->trips};
    ESP_LOGD(TAG, "After, trips has pointer %p", stop_singleton->trips);

    ESP_LOGD(TAG, "Successfully parsed stop singleton. Created stop with id %s", stop_singleton->id);

    int trip_count = cJSON_GetArraySize(trips);
    int i = 0;
    cJSON *j_trip;
    cJSON_ArrayForEach(j_trip, trips)
    {
        ESP_LOGD(TAG, "Updating schedule for trip %d", i);
        if (trip_count >= MAX_SCHEDULES)
        {
            ESP_LOGW(TAG, "Insufficient buffer for number of trips. Found %d trips. Truncating", trip_count);
            break;
        }

        cJSON *j_di = cJSON_GetObjectItemCaseSensitive(j_trip, "di");
        cJSON *j_rt = cJSON_GetObjectItemCaseSensitive(j_trip, "rt");
        ESP_LOGD(TAG, "stop_singleton trips pointer is %p", stop_singleton->trips);
        stop_singleton->trips[i] = (RealtimeSchedule){
            .id = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(j_trip, "ri")),
            .routeNumber = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(j_trip, "rs")),
            .routeName = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(j_trip, "rl")),
            .direction = j_di ? j_di->valueint : 0,
            .directionName = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(j_trip, "dn")),
            .hs = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(j_trip, "hs")),
            .rt = j_rt ? j_rt->valueint : 0,
            .stopTimes = stop_singleton->trips[i].stopTimes};

        cJSON *j_times = cJSON_GetObjectItemCaseSensitive(j_trip, "t");
        int time_count = cJSON_GetArraySize(j_times);

        int k = 0;
        cJSON *j_time;
        cJSON_ArrayForEach(j_time, j_times)
        {
            ESP_LOGD(TAG, "Updating stop %d of trip %d", k, i);
            if (k >= MAX_STOP_TIMES_PER_SCHEDULE)
            {
                ESP_LOGW(TAG, "Insufficient buffer for number of times. Found %d times for trip %d. Truncating", time_count, i);
                break;
            }
            cJSON *j_ut = cJSON_GetObjectItemCaseSensitive(j_time, "ut");
            cJSON *j_dl = cJSON_GetObjectItemCaseSensitive(j_time, "dl");
            cJSON *j_ba = cJSON_GetObjectItemCaseSensitive(j_time, "ba");
            stop_singleton->trips[i].stopTimes[k] = (RealtimeStopTime){
                .tripID = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(j_time, "ti")),
                .tripName = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(j_time, "th")),
                .departureTime = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(j_time, "dt")),
                .departureTimeUnix = j_ut ? (unsigned int)j_ut->valuedouble : 0,
                .delay = j_dl ? j_dl->valueint : 0,
                .vehicleID = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(j_time, "vi")),
                .po = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j_time, "po")),
                .do_ = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j_time, "do")),
                .no = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j_time, "no")),
                .ba = j_ba ? j_ba->valueint : 0,
                .tp = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j_time, "tp")),
                .tc = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j_time, "tc")),
                .cs = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j_time, "cs")),
                .realTime = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j_time, "rt"))};
            k++;
        }
        for (; k < MAX_STOP_TIMES_PER_SCHEDULE; k++)
        {
            // Set stop IDs to 0 to indicate that they are not valid
            stop_singleton->trips[i].stopTimes[k].tripID = INVALID_ID;
        }
        i++;
    }
    for (; i < MAX_SCHEDULES; i++)
    {
        // Set trip IDs to 0 to indicate that they are not valid
        stop_singleton->trips[i].id = INVALID_ID;
    }
    schedule_generation++;
    xSemaphoreGive(stop_singleton_mutex);

    ESP_LOGD(TAG, "Parsed into struct. E.g. trip name is: %s", stop_singleton->trips[0].stopTimes[0].tripName);

    // Swap in the new JSON object and release the previous one.
    // All char* fields in the struct point into json_singleton, so the old
    // one must not be deleted until the struct has been fully repopulated.
    cJSON_Delete(json_singleton);
    json_singleton = json;
    ESP_LOGD(TAG, "Freed JSON");
}

void _update_schedule()
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

    _parse_response_to_struct(&response);
}

void _clean_up()
{
    if (json_singleton != NULL)
    {
        cJSON_Delete(json_singleton);
        json_singleton = NULL;
    }
}

void update_schedule()
{
    while(run_update_schedule_task)
    {
        _update_schedule();
        vTaskDelay(PERIOD_MS);
    }
    _clean_up();
}
