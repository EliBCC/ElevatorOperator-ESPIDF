#include <string>
#include <vector>
#include <optional>
#include <cstdint>

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

struct Routes {
    int routeNumber;
    std::string routeDescription;
    int routeType;
};

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

struct TranslinkActivePeriod {
    std::string startTime;                  // "YYYY-MM-DDTHH:MM:SSZ"
    std::optional<std::string> endTime;     // "YYYY-MM-DDTHH:MM:SSZ" or null
};

struct TranslinkAlert {
    std::string id;
    int group;
    bool closed;
    bool critical;
    bool advisory;
    int mode;

    std::string routeId;
    std::string routeLongName;      // Human-readable name

    std::vector<int> directions;    // 0 and/or 1
    std::vector<std::string> stops; // StopID as string

    std::string stationId;          // Empty string
    std::string stationName;        // Empty string

    std::string alertLifecycle;     // "NEW"
    std::string certainty;          // "KNOWN" or "UNKNOWN"
    std::string effect;             // "SERVICE_CHANGE" or "ESCALATORS" or "CANCELATION", etc.

    int endStamp;          

    std::vector<TranslinkActivePeriod> activePeriods;

    std::string startTime;                      // "YYYY-MM-DDTHH:MM:SSZ"
    std::optional<std::string> endTime;         // "YYYY-MM-DDTHH:MM:SSZ" || null

    std::string alertText;                      // Human readable summary
    std::string header;                         // Human readable more info
    std::string description;                    // Human readable extra info

    std::string url;                            // URL for more info, or empty string
    std::string imageURLString;                 // Empty string
    std::string imageAltText;                   // Empty string

    std::string lastModified;                   // "YYYY-MM-DDTHH:MM:SSZ"
};

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
#define GET_NEXT_TRIP_FOR_ROUTE_AND_DIRECTION(route, direction) sprintf("https://getaway.translink.ca/api/gtfs/route/%d/direction/%d/nexttrip", route, direction)

struct NextTrip {
    std::string searchDateTime;             // YYYY-MM-DDTHH:MM:SS
    std::string nextTripDate;               // YYYY-MM-DD
    std::string nextTripTime;               // (h)h:MM
    unsigned int nextTripDateTimeInUnix;
    std::string sheetEndDate;               // YYYY-MM-DD
};

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
#define GET_ALL_TRIPS_FOR_ROUTE_DIRECTION_AND_DAY(route, direction, day) sprintf("https://getaway.translink.ca/api/gtfs/route/%d/direction/%d/schedules/%s", route, direction, day)

struct Direction {
    int directionID;                // di
    std::string cardinalDirection;  // cd
    std::string directionName;      // dn
};

struct StopTime {
    std::string stopTimeID;         // ti
    int stopOrder;                  // ts. Appears to indicate that this will be the Nth next bus at this stop
    std::string stopName;           // th
    std::string pn;
    std::string stopTime;           // dt. (H)H:MM
    unsigned int stopTimeUnix;      // ut
    int dt;
    bool po;
    bool do_;                       // do
    bool no;
    int ba;
    bool tp;
};

struct Stop {
    std::string id;                     // NOT the stop ID
    std::string stopID;                 // sc
    std::string stopName;               // sn
    float latitude;                     // la
    float longitude;                    // lo
    int wc;
    int ss;
    std::vector<StopTime> stopTimes;   // st
};

struct Schedule {
    std::string routeNumber;            // rs
    std::string routeName;              // rl
    int rt;
    std::vector<Direction> directions;  // d
    std::string searchDate;             // sd. YYYY-MM-DD
    std::vector<Stop> stops;            // s
};

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
#define GET_REALTIME_SCHEDULE_FOR_STOP_AND_ROUTE(stop, route, size) sprintf("https://getaway.translink.ca/api/gtfs/stop/%d/route/%d/realtimeschedules?querySize=%d")
struct RealtimeStopTime : public StopTime {
    bool tc;
    bool cs;
    bool realTime;  // rt. Indicates whether this time was updated with realtime info, or based on the schedule
};

struct RealtimeSchedule {
    std::string id;                             // ri
    std::string routeNumber;                    // rs 
    std::string routeName;                      // rl 
    int direction;                              // di
    std::string directionName;                  // dn
    std::string hs;
    int rt;
    std::vector<RealtimeStopTime> stopTimes;    // t
};

struct RealtimeStop {
    std::string id;
    std::string stopID;                     // sc
    std::string stopName;                   // sn
    float latitude;                         // la
    float longitude;                        // lo
    int wc;
    std::string searchDate;                 // sd. YYYY-MM-DDTHH:MM:SS.sssssss
    unsigned int searchDateUnix;            // lu
    std::vector<RealtimeSchedule> trips;    // r
};