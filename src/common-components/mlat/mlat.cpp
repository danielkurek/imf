#include "mlat.hpp"

#include <eigen3/Eigen/Dense>
#include <cmath>
#include <numbers>

using namespace mlat;
using namespace std;
using namespace Eigen;

static inline bool float_equal(float x, float y, float eps){
  return abs(x - y) < eps;
}

static inline float distance_2d(const position_t pos1, const position_t pos2){
  return sqrt(  pow(pos1.x - pos2.x, 2) 
              + pow(pos1.y - pos2.y, 2));
}
  
position_t MLAT::solve_single_anchor(anchor_t anchor, float result_angle){
  using namespace std::numbers;
  position_t pos;
  pos.x = anchor.pos.x + anchor.distance * cos(result_angle * (pi / 180.0));
  pos.y = anchor.pos.y + anchor.distance * sin(result_angle * (pi / 180.0));
  return pos;
}

double_solution_t MLAT::solve_two_anchors(anchor_t anchor0, anchor_t anchor1){
  // solution using equations from section "Intersection of two circles" 
  // https://paulbourke.net/geometry/circlesphere/
  double_solution_t solution{};
  constexpr float eps = 1e-5;

  const float dx = anchor1.pos.x - anchor0.pos.x;
  const float dy = anchor1.pos.y - anchor0.pos.y;

  const float dist = distance_2d(anchor0.pos, anchor1.pos);
  
  // check if circles are intersecting
  if(dist > anchor0.distance + anchor1.distance){
    // circles are separate
    solution.valid = false;
    return solution;
  }
  if(dist < abs(anchor0.distance - anchor1.distance)){
    // one circle is contained within the the other
    solution.valid = false;
    return solution;
  }
  if(float_equal(dist, 0.0, eps) && float_equal(anchor0.distance, anchor1.distance, eps)){
    // circles are coincident, infinite solutions
    solution.valid = false;
    return solution;
  }

  if(float_equal(dist, 0.0, eps)){
    // prevent division by zero
    solution.valid = false;
    return solution
  }

  const float a = (pow(anchor0.distance, 2) - pow(anchor1.distance, 2) + pow(dist, 2)) / (2*dist);
  const float h = sqrt(pow(anchor0.distance, 2) - pow(a, 2));

  const float x2 = anchor0.pos.x + (a*dx)/dist;
  const float y2 = anchor0.pos.y + (a*dy)/dist;

  solution.pos1.x = x2 + (h*dy)/dist;
  solution.pos1.y = y2 - (h*dx)/dist;

  solution.pos2.x = x2 - (h*dy)/dist;
  solution.pos2.y = y2 + (h*dx)/dist;
  
  solution.valid = true;
  return solution;
}
solution_t MLAT::solve(const vector<anchor_t>& anchors){
  solution_t solution{};
  if(anchors.size() < 3) {
    solution.valid = false;
    return solution;
  }
  MatrixXf A(anchors.size() - 1, 2);
  for(size_t i = 1; i < anchors.size(); i++){
    A(i-1, 0) = anchors[i].pos.x - anchors[0].pos.x;
    A(i-1, 1) = anchors[i].pos.y - anchors[0].pos.y;
  }

  VectorXf b(A.rows());
  for(size_t i = 1; i < anchors.size(); i++){
    b(i-1) = 0.5 * (
        pow(anchors[0].distance,2) - pow(anchors[i].distance,2) 
      + (pow(anchors[i].pos.x,2) + pow(anchors[i].pos.y, 2))
      - (pow(anchors[0].pos.x,2) + pow(anchors[0].pos.y, 2))
      );
  }
  VectorXf x = A.completeOrthogonalDecomposition().solve(b);
  solution.pos.x = x(0);
  solution.pos.y = x(1);
  solution.error = ((A*x) - b).norm();
  solution.valid = true;
  return solution;
}