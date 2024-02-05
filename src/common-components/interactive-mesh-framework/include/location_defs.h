#ifndef LOCATION_DEFS_H_
#define LOCATION_DEFS_H_

#include "inttypes.h"

#ifdef __cplusplus
extern "C"{
#endif

typedef struct{
    int16_t  local_north;
    int16_t  local_east;
    int16_t  local_altitude;
    uint8_t  floor_number;
    uint16_t uncertainty;
} location_local_t;

#ifdef __cplusplus
}
#endif

#endif