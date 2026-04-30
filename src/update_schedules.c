#include <stdio.h>
#include <stdlib.h>

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

    http_get_task((void *)(&request));
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

    // Strip square brackets, if present
    int lhs_offset = 0;
    int rhs_offset = 0;
    if(raw_response_buffer[0] == '[')
    {
        lhs_offset = 1;
    }
    if(raw_response_buffer[response.written - 1] == ']')
    {
        rhs_offset = 1;
    }

    // Parse into JSON
    cJSON *json = cJSON_ParseWithLength(raw_response_buffer + 1, response.written - lhs_offset - rhs_offset);
    char *formatted_json = cJSON_Print(json);
    ESP_LOGD(TAG,"JSON: \n%s\n", formatted_json);

    cJSON *id = cJSON_GetObjectItemCaseSensitive(json, "id");
    cJSON *stopID = cJSON_GetObjectItemCaseSensitive(json, "sc");
    cJSON *stopName = cJSON_GetObjectItemCaseSensitive(json, "sn");
    cJSON *latitude = cJSON_GetObjectItemCaseSensitive(json, "la");
    cJSON *longitude = cJSON_GetObjectItemCaseSensitive(json, "lo");
    cJSON *wc = cJSON_GetObjectItemCaseSensitive(json, "wc");
    cJSON *searchDate = cJSON_GetObjectItemCaseSensitive(json, "sd");
    cJSON *searchDateUnix = cJSON_GetObjectItemCaseSensitive(json, "lu");
    cJSON *trips = cJSON_GetObjectItemCaseSensitive(json, "r");

    RealtimeStop stop = {
        .id            = cJSON_GetStringValue(id),        // don't use cJSON_Print
        .stopID        = cJSON_GetStringValue(stopID),
        .stopName      = cJSON_GetStringValue(stopName),
        .latitude      = (float)latitude->valuedouble,    // direct field access
        .longitude     = (float)longitude->valuedouble,
        .wc            = wc->valueint,
        .searchDate    = cJSON_GetStringValue(searchDate),
        .searchDateUnix = (unsigned int)searchDateUnix->valuedouble,
        .trips         = NULL
    };

    int trip_count = cJSON_GetArraySize(trips);
    RealtimeSchedule *schedules = malloc(trip_count * sizeof(RealtimeSchedule));

    int i = 0;
    cJSON *j_trip;
    cJSON_ArrayForEach(j_trip, trips)
    {
        cJSON *j_times = cJSON_GetObjectItemCaseSensitive(j_trip, "t");
        int time_count = cJSON_GetArraySize(j_times);
        RealtimeStopTime *stop_times = malloc(time_count * sizeof(RealtimeStopTime));

        int k = 0;
        cJSON *j_time;
        cJSON_ArrayForEach(j_time, j_times)
        {
            cJSON *j_ut = cJSON_GetObjectItemCaseSensitive(j_time, "ut");
            cJSON *j_dl = cJSON_GetObjectItemCaseSensitive(j_time, "dl");
            cJSON *j_ba = cJSON_GetObjectItemCaseSensitive(j_time, "ba");
            stop_times[k++] = (RealtimeStopTime){
                .tripID            = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(j_time, "ti")),
                .tripName          = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(j_time, "th")),
                .departureTime     = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(j_time, "dt")),
                .departureTimeUnix = j_ut ? (unsigned int)j_ut->valuedouble : 0,
                .delay             = j_dl ? j_dl->valueint : 0,
                .vehicleID         = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(j_time, "vi")),
                .po                = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j_time, "po")),
                .do_               = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j_time, "do")),
                .no                = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j_time, "no")),
                .ba                = j_ba ? j_ba->valueint : 0,
                .tp                = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j_time, "tp")),
                .tc                = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j_time, "tc")),
                .cs                = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j_time, "cs")),
                .realTime          = cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(j_time, "rt"))
            };
        }

        cJSON *j_di = cJSON_GetObjectItemCaseSensitive(j_trip, "di");
        cJSON *j_rt = cJSON_GetObjectItemCaseSensitive(j_trip, "rt");
        schedules[i++] = (RealtimeSchedule){
            .id            = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(j_trip, "ri")),
            .routeNumber   = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(j_trip, "rs")),
            .routeName     = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(j_trip, "rl")),
            .direction     = j_di ? j_di->valueint : 0,
            .directionName = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(j_trip, "dn")),
            .hs            = cJSON_GetStringValue(cJSON_GetObjectItemCaseSensitive(j_trip, "hs")),
            .rt            = j_rt ? j_rt->valueint : 0,
            .stopTimes     = stop_times
        };
    }

    stop.trips = schedules;

    ESP_LOGD(TAG, "Parsed into struct. E.g. trip name is: %s", stop.trips[0].stopTimes[0].tripName);

    for (int j = 0; j < trip_count; j++)
        free(schedules[j].stopTimes);
    free(schedules);
    cJSON_Delete(json);
    free(formatted_json);

    vTaskDelete(NULL);
}
