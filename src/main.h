#define WIFI_TIMEOUT_MS     10 * 1000

#define BAUD_RATE           11520

typedef struct {
    char *buf;
    size_t size;
    size_t written;
} http_response_buf_t;