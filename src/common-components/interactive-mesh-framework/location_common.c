#include "location_common.h"
#include <stdio.h>

esp_err_t simple_str_to_loc(const char* str, location_local_t *loc_local){
    int ret = sscanf(str, "N%04" SCNd16 "E%04" SCNd16, &(loc_local->local_north), &(loc_local->local_east));
    if(ret == 2){
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t simple_loc_to_str(const location_local_t* loc_local, size_t buf_len, char buf[]){
    int ret = snprintf(buf, buf_len, "N%04" PRId16 "E%04" PRId16, loc_local->local_north, loc_local->local_east);
    if(ret > 0){
        return ESP_OK;
    }
    return ESP_FAIL;
}