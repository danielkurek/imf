/**
 * @file location_common.c
 * @author Daniel Kurek (daniel.kurek.dev@gmail.com)
 * @brief Implementation of @ref location_common.h
 * @version 0.1
 * @date 2023-12-09
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#include "location_common.h"
#include <stdio.h>
#include <math.h>

esp_err_t simple_str_to_loc(const char* str, location_local_t *loc_local){
    int ret = sscanf(str, "N%05" SCNd16 "E%05" SCNd16 "A%05" SCNd16 "F%03" SCNu8 "U%05" SCNu16, 
        &(loc_local->local_north), 
        &(loc_local->local_east),
        &(loc_local->local_altitude),
        &(loc_local->floor_number),
        &(loc_local->uncertainty));
    if(ret == 5){
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t simple_loc_to_str(const location_local_t* loc_local, size_t buf_len, char buf[]){
    int ret = snprintf(buf, buf_len, "N%05" PRId16 "E%05" PRId16 "A%05" PRId16 "F%03" PRIu8 "U%05" PRIu16, 
        loc_local->local_north,
        loc_local->local_east,
        loc_local->local_altitude,
        loc_local->floor_number,
        loc_local->uncertainty);
    if(ret > 0){
        return ESP_OK;
    }
    return ESP_FAIL;
}

float loc_distance(const location_local_t* loc_local1, const location_local_t* loc_local2){
    float dist_north = loc_local1->local_north - loc_local2->local_north;
    float dist_east = loc_local1->local_east - loc_local2->local_east;
    return sqrt(pow(dist_north, 2) + pow(dist_east, 2));
}