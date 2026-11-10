#ifndef ROUTE_CONFIG_H
#define ROUTE_CONFIG_H

#include <stddef.h>
#include <stdint.h>

#define MAX_ROUTE_CONFIGS 4
#define MAX_ROUTE_NUMBERS_PER_CONFIG 4

typedef struct {
    uint32_t stop_number;
    const char *name;
    const uint32_t *route_numbers;
    size_t route_count;
} RouteConfig;

static const uint32_t ROUTES_84[] = { 84 };
static const uint32_t ROUTES_50179[] = { 3, 8, 19 };

static const RouteConfig ROUTE_CONFIGS[] = {
    { .stop_number = 59760, .name = "84 East", .route_numbers = ROUTES_84, .route_count = 1 },
    { .stop_number = 59761, .name = "84 West", .route_numbers = ROUTES_84, .route_count = 1 },
    { .stop_number = 50179, .name = "MainN", .route_numbers = ROUTES_50179, .route_count = 3 },
    { .stop_number = 59580, .name = "MainS", .route_numbers = ROUTES_50179, .route_count = 3 },
};

#define ROUTE_CONFIG_COUNT (sizeof(ROUTE_CONFIGS) / sizeof(ROUTE_CONFIGS[0]))
_Static_assert(ROUTE_CONFIG_COUNT <= MAX_ROUTE_CONFIGS, "Too many route configs");
_Static_assert(sizeof(ROUTES_50179) / sizeof(ROUTES_50179[0]) <= MAX_ROUTE_NUMBERS_PER_CONFIG,
               "Too many route numbers for one stop");

#endif
