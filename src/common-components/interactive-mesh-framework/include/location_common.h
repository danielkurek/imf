#ifndef LOCATION_COMMON_H_
#define LOCATION_COMMON_H_

#include "location_defs.h"
#include "inttypes.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C"{
#endif

#define SIMPLE_LOC_STR_LEN 3*(6+1) + (3+1) + (5+1) + 1

esp_err_t simple_str_to_loc(const char* str, location_local_t *loc_local);
esp_err_t simple_loc_to_str(const location_local_t* loc_local, size_t buf_len, char buf[]);
float loc_distance(const location_local_t* loc_local1, const location_local_t* loc_local2);

#ifdef __cplusplus
}
#endif

#endif