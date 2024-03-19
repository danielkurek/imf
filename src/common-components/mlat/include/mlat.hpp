#ifndef MLAT_H
#define MLAT_H

#include <vector>

namespace mlat {
    typedef struct{
        float x,y;
    } position_t;
    
    typedef struct{
        position_t pos;
        float distance;
    } anchor_t;

    typedef struct{
        position_t pos;
        float error;
        bool valid;
    } solution_t;

    typedef struct{
        position_t pos1;
        position_t pos2;
        bool valid;
    } double_solution_t;

    class MLAT {
        public:
            static position_t solve_single_anchor(anchor_t anchor, float result_angle);
            static double_solution_t solve_two_anchors(anchor_t anchor0, anchor_t anchor1);
            static solution_t solve(const std::vector<anchor_t>& anchors);
    };
}

#endif /* MLAT_H */