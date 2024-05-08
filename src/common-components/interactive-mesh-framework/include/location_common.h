/**
 * @file location_common.h
 * @author Daniel Kurek (daniel.kurek.dev@gmail.com)
 * @brief Helper functions for location
 * @version 0.1
 * @date 2023-12-09
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef LOCATION_COMMON_H_
#define LOCATION_COMMON_H_

#include "location_defs.h"
#include "inttypes.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C"{
#endif

/**
 * @brief length of string representation of location_local_t
 */
#define SIMPLE_LOC_STR_LEN 3*(6+1) + (3+1) + (5+1) + 1

/**
 * @brief parse string representation of location_local_t
 * 
 * @param[in] str string representation of location_local_t
 * @param[out] loc_local parsed location
 * @return esp_err_t ESP_OK if parsing was successful
 */
esp_err_t simple_str_to_loc(const char* str, location_local_t *loc_local);

/**
 * @brief Create string representation of location_local_t
 * 
 * @param[in] loc_local location to convert to string
 * @param[in] buf_len length of @p buf
 * @param[out] buf output buffer
 * @return esp_err_t ESP_OK on success
 */
esp_err_t simple_loc_to_str(const location_local_t* loc_local, size_t buf_len, char buf[]);

/**
 * @brief 2D Euclidean distance between two locations
 * 
 * @param loc_local1 first location
 * @param loc_local2 second location
 * @return float resulting 2D distance
 */
float loc_distance(const location_local_t* loc_local1, const location_local_t* loc_local2);

#ifdef __cplusplus
}
#endif

#endif