#ifndef UPDATE_SCHEDULES_H
#define UPDATE_SCHEDULES_H

#include "translink.h"

#define PERIOD_MS 30 * 1000 / portTICK_PERIOD_MS

#define MAX_SCHEDULES 5
#define MAX_STOP_TIMES_PER_SCHEDULE 5
extern RealtimeStop *stop_singleton;
#define MAX_STOP_SINGETON_MUTEX_WAIT_MS 1000
extern SemaphoreHandle_t stop_singleton_mutex;
extern uint32_t schedule_generation;

#define INVALID_ID '\0'      // IDs used to mark singleton objects as not valid, when the full array is not used

void init_schedule_updater();
void update_schedule();
void cleanup_schedule_updater();

extern bool run_update_schedule_task;

#endif