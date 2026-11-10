#ifndef ROUTE_CONFIG_H
#define ROUTE_CONFIG_H

#include <stdint.h>

typedef struct {
    uint32_t stop_number;
    uint32_t route_number;
    const char *name;
} RouteConfig;

#define MAX_ROUTE_CONFIGS 4
#define ROUTE_CONFIG_COUNT 1

static const RouteConfig ROUTE_CONFIGS[MAX_ROUTE_CONFIGS] = {
    { .stop_number = 59760, .route_number = 84, .name = "84 East" },
};

#endif
