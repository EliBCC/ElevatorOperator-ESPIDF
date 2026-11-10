#ifndef ROUTE_CONFIG_H
#define ROUTE_CONFIG_H

#include <stdint.h>

typedef struct {
    uint32_t stop_number;
    uint32_t route_number;
    const char *name;
} RouteConfig;

#define MAX_ROUTE_CONFIGS 4

static const RouteConfig ROUTE_CONFIGS[] = {
    { .stop_number = 59760, .route_number = 84, .name = "84 East" },
    { .stop_number = 59761, .route_number = 84, .name = "84 West" },
    { .stop_number = 50179, .route_number = 84, .name = "3 North" },
    { .stop_number = 59580, .route_number = 84, .name = "3 South" },
};

#define ROUTE_CONFIG_COUNT (sizeof(ROUTE_CONFIGS) / sizeof(ROUTE_CONFIGS[0]))
_Static_assert(ROUTE_CONFIG_COUNT <= MAX_ROUTE_CONFIGS, "Too many route configs");

#endif
