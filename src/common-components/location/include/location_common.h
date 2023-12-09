#ifndef LOCATION_COMMON_H_
#define LOCATION_COMMON_H_

#include "location_defs.h"
#include "inttypes.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C"{
#endif

esp_err_t simple_str_to_loc(const char* str, location_local_t *loc_local);
esp_err_t simple_loc_to_str(const location_local_t* loc_local, size_t buf_len, char buf[]);

#ifdef __cplusplus
}
#endif

#endif