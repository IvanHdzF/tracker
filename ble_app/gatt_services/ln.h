#ifndef SERVICE_LN_H
#define SERVICE_LN_H

#include <stdint.h>
#include "cts.h"

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