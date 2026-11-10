#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// TransLink URLs

/*
Format:
[
    {
        "routeNumber": int,
        "routeDescription": string,
        "routeType": int
    },
    ...
]
*/
#define GET_ALL_ROUTES "https://getaway.translink.ca/api/gtfs/routes"

typedef struct {
    int routeNumber;
    char *routeDescription;
    int routeType;
} Routes;

/*
Format:
[
    {
        "id": string,
        "group": int,
        "closed": bool,
        "critical": bool,
        "advisory": bool,
        "mode":int,
        "routeId": string,
        "routeLongName": string,
        "directions": int[],
        "stops": string[],
        "stationId": string,
        "stationName": string,
        "alertLifecycle": string,
        "certainty": string,
        "effect": string,
        "endStamp": int,
        "activePeriods": [
            {
                "startTime": datetime,      // tz aware
                "endTime": datetime || null // tz aware
            }
        ],
        "startTime": datetime,              // tz aware
        "endTime": datetime || null,        // tz aware
        "alertText": string,
        "header": string,
        "description": string,
        "url": string,
        "imageURLString": string,
        "imageAltText": "",
        "lastModified": datetime            // tz aware
    },
    ...
]
*/
#define GET_ALL_ALERTS "https://getaway.translink.ca/api/allalerts"

typedef struct {
    char * startTime;                  // "YYYY-MM-DDTHH:MM:SSZ"
    char * endTime;     // "YYYY-MM-DDTHH:MM:SSZ" or null
} TranslinkActivePeriod;

typedef struct {
    char * id;
    int group;
    bool closed;
    bool critical;
    bool advisory;
    int mode;

    char * routeId;
    char * routeLongName;      // Human-readable name

    int *directions;    // 0 and/or 1
    char ** stops;      // StopID as string

    char * stationId;          // Empty string
    char * stationName;        // Empty string

    char * alertLifecycle;     // "NEW"
    char * certainty;          // "KNOWN" or "UNKNOWN"
    char * effect;             // "SERVICE_CHANGE" or "ESCALATORS" or "CANCELATION", etc.

    int endStamp;          

    TranslinkActivePeriod * activePeriods;

    char * startTime;                      // "YYYY-MM-DDTHH:MM:SSZ"
    char * endTime;                        // "YYYY-MM-DDTHH:MM:SSZ" || null

    char * alertText;                      // Human readable summary
    char * header;                         // Human readable more info
    char * description;                    // Human readable extra info

    char * url;                            // URL for more info, or empty string
    char * imageURLString;                 // Empty string
    char * imageAltText;                   // Empty string

    char * lastModified;                   // "YYYY-MM-DDTHH:MM:SSZ"
} TranslinkAlert;

/*
Format:
{
    "searchDateTime": datetime, // Not tz aware
    "nextTripDate": datetime,   // "YYYY-MM-DD"
    "nextTripTime": string,     // "HH:MM"
    "nextTripDateTimeInUnix": unsigned int,
    "sheetEndDate": datetime    // "YYYY-MM-DD"
}
*/
#define GET_NEXT_TRIP_FOR_ROUTE_AND_DIRECTION(buff, route, direction) sprintf(buff, "https://getaway.translink.ca/api/gtfs/route/%d/direction/%d/nexttrip", route, direction)

typedef struct {
    char * searchDateTime;             // YYYY-MM-DDTHH:MM:SS
    char * nextTripDate;               // YYYY-MM-DD
    char * nextTripTime;               // (h)h:MM
    unsigned int nextTripDateTimeInUnix;
    char * sheetEndDate;               // YYYY-MM-DD
} NextTrip;

/*
Format:
{
    "rs": string,
    "rl": string,
    "rt": int,
    "d": [
        {
            "di": int,
            "cd": string,
            "dn": string
        },
        ...
    ],
    "sd": string,
    "s": [
        {
            "id": string,
            "sc": string,
            "sn": string,
            "la": float,
            "lo": float,
            "wc": int,
            "ss": int,
            "st": [
                {
                    "ti": int,
                    "ts": int,
                    "th": string,
                    "pn": string,
                    "dt": string,
                    "ut": unsigned it,
                    "dl": int,
                    "po": bool,
                    "do": bool,
                    "no": bool,
                    "ba": int,
                    "tp": bool
                },
                ...
            ],
        }
    ]
}            
*/

#define GET_ALL_TRIPS_FOR_ROUTE_DIRECTION_AND_DAY(buff, route, direction, day) sprintf(buff, "https://getaway.translink.ca/api/gtfs/route/%d/direction/%d/schedules/%s", route, direction, day)

typedef struct {
    int directionID;                // di
    char * cardinalDirection;  // cd
    char * directionName;      // dn
} Direction;

typedef struct {
    char * stopTimeID;         // ti
    int stopOrder;                  // ts. Appears to indicate that this will be the Nth next bus at this stop
    char * stopName;           // th
    char * pn;
    char * stopTime;           // dt. (H)H:MM
    unsigned int stopTimeUnix;      // ut
    int dt;
    bool po;
    bool do_;                       // do
    bool no;
    int ba;
    bool tp;
} StopTime;

typedef struct {
    char * id;                     // NOT the stop ID
    char * stopID;                 // sc
    char * stopName;               // sn
    float latitude;                     // la
    float longitude;                    // lo
    int wc;
    int ss;
    StopTime * stopTimes;   // st
} Stop;

typedef struct {
    char * routeNumber;            // rs
    char * routeName;              // rl
    int rt;
    Direction * directions;  // d
    char * searchDate;             // sd. YYYY-MM-DD
    Stop * stops;            // s
} Schedule;

/*
Format:
[
    {
        "id": string,
        "sc": string,
        "sn": string,
        "la": float,
        "lo": float,
        "wc": int,
        "sd": datetime,
        "lu": unsigned int,
        "r": [
            {
                "ri": string,
                "rs": string,
                "rl": string,
                "di": int,
                "dn": string,
                "hs": string,
                "rt": int,
                "t": [
                    {
                        "ti": string,
                        "th": string,
                        "dt": string,
                        "ut": unsigned int,
                        "dl": int,
                        "vi": string,
                        "po": bool,
                        "do": bool,
                        "no": bool,
                        "ba": int,
                        "tp": bool,
                        "tc": bool,
                        "cs": bool,
                        "rt": bool
                    },
                    ...
                ]
            }
        ]
    }
]

*/
#define GET_REALTIME_SCHEDULE_FOR_STOP_AND_ROUTE(buff, stop, route, size) sprintf(buff, "https://getaway.translink.ca/api/gtfs/stop/%d/route/%d/realtimeschedules?querySize=%d", stop, route, size)
typedef struct {
    uint32_t departure_time_unix;
    uint32_t route_number;
} RealtimeDeparture;

typedef struct {
    uint32_t search_date_unix;
    size_t route_config_index;
    size_t departure_count;
    RealtimeDeparture *departures;
} RealtimeStop;