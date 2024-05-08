/**
 * @file mlat.hpp
 * @author Daniel Kurek (daniel.kurek.dev@gmail.com)
 * @brief Calculation of multilateration using Least squares method
 * @version 0.1
 * @date 2024-03-19
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#ifndef MLAT_H
#define MLAT_H

#include <vector>

namespace mlat {
    typedef struct{
        float x; /**< x coordinate */
        float y; /**< y coordinate */
    } position_t;
    
    typedef struct{
        position_t pos; /**< anchor position */
        float distance; /**< distance to anchor */
    } anchor_t;

    typedef struct{
        position_t pos; /**< resulting position */
        float error; /**< least squares error */
        bool valid; /**< true if resulting position was found */
    } solution_t;
    
    typedef struct{
        position_t pos1; /**< first intersection */
        position_t pos2; /**< second intersection */
        bool valid; /**< true if intersections were found */
    } double_solution_t;

    class MLAT {
        public:
            /**
             * @brief Get point on circle given an angle
             * 
             * @param anchor definition of circle
             * @param result_angle angle of resulting position
             * @return position_t resulting position
             */
            static position_t solve_single_anchor(anchor_t anchor, float result_angle);

            /**
             * @brief Calculates intersection of 2 circles
             * 
             * @param anchor0 first circle
             * @param anchor1 second circle
             * @return double_solution_t two intersection points or invalid result
             */
            static double_solution_t solve_two_anchors(anchor_t anchor0, anchor_t anchor1);

            /**
             * @brief Solve multilateration using Least squares method
             * 
             * @param anchors anchors that define circles for multilateration
             * @return solution_t resulting position
             */
            static solution_t solve(const std::vector<anchor_t>& anchors);
    };
}

#endif /* MLAT_H */