/**
 * @file location_defs.h
 * @author Daniel Kurek (daniel.kurek.dev@gmail.com)
 * @brief Definition of location (from Bluetooth mesh)
 * @version 0.1
 * @date 2023-12-08
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#ifndef LOCATION_DEFS_H_
#define LOCATION_DEFS_H_

#include "inttypes.h"

#ifdef __cplusplus
extern "C"{
#endif

typedef struct{
    int16_t  local_north; /**< local coordinate in north direction */
    int16_t  local_east; /**< local coordinate in east direction */
    int16_t  local_altitude; /**< local altitude */
    uint8_t  floor_number; /**< floor number */
    uint16_t uncertainty; /**< uncertainty of the location */
} location_local_t;

#ifdef __cplusplus
}
#endif

#endif