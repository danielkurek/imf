/**
 * @file hsl.h
 * @author Daniel Kurek (daniel.kurek.dev@gmail.com)
 * @brief Wrapper for @ref hsl_github.h
 * @version 0.1
 * @date 2023-08-19
 * 
 * @copyright Copyright (c) 2023
 * 
 */

#ifndef HSL_H_
#define HSL_H_

#include "color/color.h"

#ifdef __cplusplus
extern "C"{
#endif

/**
 * @brief Convert hsl to rgb
 * 
 * @param hsl input color in HSL color space
 * @return rgb_t converted color in RGB color space
 */

rgb_t hsl_to_rgb(hsl_t hsl);

/**
 * @brief Convert rgb to hsl
 * 
 * @param rgb input color in RGB color space
 * @return hsl_t converted color in HSL color space
 */
hsl_t rgb_to_hsl(rgb_t rgb);

#ifdef __cplusplus
}
#endif

#endif