#ifndef FETCH_HTTP_H
#define FETCH_HTTP_H

#include <stddef.h>

#define WIFI_TIMEOUT_MS     10 * 1000
#define MAX_FAILURES   10
#define MAX_HTTP_OUTPUT_BUFFER 2048
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

typedef struct {
    char *buf;
    size_t size;
    size_t written;
} http_response_buf_t;

void wifi_init_sta(void);
void http_get_task(void *pvParameters);
#endif