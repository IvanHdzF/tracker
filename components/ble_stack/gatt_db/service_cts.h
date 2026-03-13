#ifndef SERVICE_CTS_H
#define SERVICE_CTS_H

#include <stdint.h>

typedef enum
{
  WEEK_DAY_UNKNOWN = 0,
  WEEK_DAY_MONDAY,
  WEEK_DAY_TUESDAY,
  WEEK_DAY_WEDNESDAY,
  WEEK_DAY_THURSDAY,
  WEEK_DAY_FRIDAY,
  WEEK_DAY_SATURDAY,
  WEEK_DAY_SUNDAY,
} cts_day_of_week_t;

typedef enum
{
  CTS_REASON_MANUAL_TIME_UPDATE = 0x01,
  CTS_REASON_EXT_REF_TIME_UPDATE = 0x02,
  CTS_REASON_TIMEZONE_CHANGE = 0x04,
  CTS_REASON_DST_CHANGE = 0x08,
} cts_adjust_reason_t;

typedef struct __attribute__((packed))
{
  /* Year as defined by the Gregorian calendar.
  Valid range 1582 to 9999. A value of 0 means that the year is not known.
  All other values are Reserved for Future Use */
  uint16_t year;
  /* Month of the year as defined by the Gregorian calendar.
  Valid range 1 (January) to 12 (December). A value of 0
  means that the month is not known. All other values are
  Reserved for Future Use. */
  uint8_t month;
  /* Day of the month as defined by the Gregorian calendar.
  Valid range 1 to 31. A value of 0 means that the day of
  month is not known. All other values are Reserved for
  Future Use */
  uint8_t day;
  /* Number of hours past midnight. Valid range 0 to 23. All
  other values are Reserved for Future Use */
  uint8_t hours;
  /* Number of minutes since the start of the hour. Valid
  range 0 to 59. All other values are Reserved for Future
  Use. */
  uint8_t minutes;
  /* Number of seconds since the start of the minute. Valid
  range 0 to 59. All other values are Reserved for Future
  Use. */
  uint8_t seconds;
} date_time_t;

typedef struct __attribute__((packed))
{
  date_time_t date_time;
  uint8_t day_of_week;
} day_date_time_t;

typedef struct __attribute__((packed))
{
  day_date_time_t day_date_time;
  uint8_t fractions_256;
} exact_time_365_t;

typedef struct __attribute__((packed))
{
  exact_time_365_t exact_time;
  uint8_t adjust_reason;
} current_time_t;








//TODO: Move out this struct to app layer

#define LNF_FLAGS_MASK                 (0x7F)
#define LOC_AND_SPEED_DATA_FLAGS_MASK  (0x7F)
#define POSITION_QUALITY_FLAGS_MASK    (0x7F)

typedef struct __attribute__((packed))
{
  uint16_t flags;
  uint16_t instant_speed;
  uint8_t total_distance[3];
  int32_t location_latitude;
  int32_t location_longitude;
  int8_t elevation[3];
  uint16_t heading;
  uint8_t rolling_time;
  date_time_t date_time;
} loc_and_speed_data_t;

typedef struct __attribute__((packed))
{
  uint16_t flags;
  uint8_t beacon_num_sol;
  uint8_t beacon_num_view;
  uint16_t time_to_first_fix;
  uint32_t ephe;
  uint32_t evpe;
  uint8_t hdop;
  uint8_t vdop;
} pos_qual_t;

#endif /* SERVICE_CID_H */