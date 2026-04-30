#include <stdio.h>
#include <stdlib.h>

#include "esp_log.h"
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

    // cJSON *id = cJSON_GetObjectItemCaseSensitive(json, "id");
    // cJSON *stopID = cJSON_GetObjectItemCaseSensitive(json, "sc");
    // cJSON *stopName = cJSON_GetObjectItemCaseSensitive(json, "sn");
    // cJSON *latitude = cJSON_GetObjectItemCaseSensitive(json, "la");
    // cJSON *longitude = cJSON_GetObjectItemCaseSensitive(json, "lo");
    // cJSON *wc = cJSON_GetObjectItemCaseSensitive(json, "wc");
    // cJSON *searchDate = cJSON_GetObjectItemCaseSensitive(json, "sd");
    // cJSON *searchDateUnix = cJSON_GetObjectItemCaseSensitive(json, "lu");
    // cJSON *trips = cJSON_GetObjectItemCaseSensitive(json, "r");

    // RealtimeStop stop = {
    //     .id = cJSON_Print(id),
    //     .stopID = cJSON_Print(stopID),
    //     .stopName = cJSON_Print(stopName),
    //     .latitude = atof(cJSON_Print(latitude)),
    //     .longitude = atof(cJSON_Print(longitude)),
    //     .wc = atoi(cJSON_Print(wc)),
    //     .searchDate = cJSON_Print(searchDate),
    //     .searchDateUnix = atoi(cJSON_Print(searchDateUnix)),
    //     .trips = (RealtimeSchedule *)(0)
    // };

    // RealtimeSchedule schedules 
    // cJSON *subitem;
    // for (int i=0; i < cJSON_GetArraySize(trips); i++)
    // {
    //     subitem = cJSON_GetArrayItem(trips, i);
    //     cJSON *id = cJSON_GetObjectItemCaseSensitive(subitem, "ri");
    //     cJSON *routeNumber = cJSON_GetObjectItemCaseSensitive(subitem, "rs");
    //     cJSON *routeName = cJSON_GetObjectItemCaseSensitive(subitem, "rl");
    //     cJSON *direction = cJSON_GetObjectItemCaseSensitive(subitem, "di");
    //     cJSON *directionName = cJSON_GetObjectItemCaseSensitive(subitem, "dn");
    //     cJSON *hs = cJSON_GetObjectItemCaseSensitive(subitem, "hs");
    //     cJSON *rt = cJSON_GetObjectItemCaseSensitive(subitem, "t");

    //     RealtimeSchedule schedule = {}
    // }

    cJSON_Delete(json);
    free(formatted_json);

    vTaskDelete(NULL);
}
