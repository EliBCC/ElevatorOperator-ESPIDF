#ifndef UPDATE_SCHEDULES_H
#define UPDATE_SCHEDULES_H

#include "translink.h"

#define MAX_SCHEDULES 5
#define MAX_STOP_TIMES_PER_SCHEDULE 5
extern RealtimeStop *stop_singleton;

#define INVALID_ID '\0'      // IDs used to mark singleton objects as not valid, when the full array is not used

void init_schedule_updater();
void update_schedule();
void cleanup_schedule_updater();

#endif