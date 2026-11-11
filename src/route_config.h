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

#define DEFINE_ROUTE_CONFIG(stop, label, routes) \
        { .stop_number = (stop), .name = (label), .route_numbers = (routes), \
            .route_count = sizeof(routes) / sizeof((routes)[0]) }

static const RouteConfig ROUTE_CONFIGS[] = {
        DEFINE_ROUTE_CONFIG(59760, "84 East", ROUTES_84),
        DEFINE_ROUTE_CONFIG(59761, "84 West", ROUTES_84),
        DEFINE_ROUTE_CONFIG(50179, "MainN", ROUTES_50179),
        DEFINE_ROUTE_CONFIG(59580, "MainS", ROUTES_50179),
};

#define ROUTE_CONFIG_COUNT (sizeof(ROUTE_CONFIGS) / sizeof(ROUTE_CONFIGS[0]))
_Static_assert(ROUTE_CONFIG_COUNT <= MAX_ROUTE_CONFIGS, "Too many route configs");
_Static_assert(sizeof(ROUTES_84) / sizeof(ROUTES_84[0]) <= MAX_ROUTE_NUMBERS_PER_CONFIG,
                             "Too many route numbers for a stop");
_Static_assert(sizeof(ROUTES_50179) / sizeof(ROUTES_50179[0]) <= MAX_ROUTE_NUMBERS_PER_CONFIG,
                             "Too many route numbers for a stop");

#undef DEFINE_ROUTE_CONFIG

#endif
