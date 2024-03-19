#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <iostream>
#include <random>
#include <cmath>


#include "mlat.hpp"

using namespace std;
using namespace mlat;

static inline float distance_2d(const position_t pos1, const position_t pos2){
  return sqrt(  pow(pos1.x - pos2.x, 2) 
              + pow(pos1.y - pos2.y, 2));
}

static void print_solution(const solution_t sol){
  if(sol.valid){
    cout << "x=" <<to_string(sol.pos.x) << " y=" << to_string(sol.pos.y) << " err=" << to_string(sol.error) << endl;
  } else{
    cout << "invalid solution" << endl;
  }
}

static void print_position(const position_t pos){
  cout << "x=" <<to_string(pos.x) << " y=" << to_string(pos.y) << endl;
}

static void print_anchors(const std::vector<anchor_t>& anchors){
  for(auto && anchor : anchors){
    cout << "x=" <<to_string(anchor.pos.x) << " y=" << to_string(anchor.pos.y) << "dist="<< to_string(anchor.distance) << endl;
  }
}

void mlat_test(void *data) {
  random_device rand;
  default_random_engine rande(rand());
  uniform_real_distribution<float> randunif(0, 1);

  float W = 9, L = 9;
  std::vector<anchor_t> anchors;
  anchors.emplace_back((position_t){0, 0}, 0.0);
  anchors.emplace_back((position_t){W, 0}, 0.0);
  anchors.emplace_back((position_t){W, L}, 0.0);
  anchors.emplace_back((position_t){0, L}, 0.0);
  anchors.emplace_back((position_t){2, 3}, 0.0);
  anchors.emplace_back((position_t){3, 2}, 0.0);
  anchors.emplace_back((position_t){1, 9}, 0.0);
  anchors.emplace_back((position_t){8, 2}, 0.0);
  anchors.emplace_back((position_t){3, 15}, 0.0);
  anchors.emplace_back((position_t){15, 0}, 0.0);

  position_t node_pos{W * randunif(rande), L * randunif(rande)};
  
  for (size_t i = 0; i < anchors.size(); i++) {
    anchors[i].distance = distance_2d(anchors[i].pos, node_pos);
  }
  
  solution_t solution_pure = MLAT::solve(anchors);

  cout << "Anchors without errors" << endl;
  print_anchors(anchors);
  cout << "Node" << endl;
  print_position(node_pos);
  cout << "Solution" << endl;
  print_solution(solution_pure);

  double error = 0.5;
  for (size_t i = 0; i < anchors.size(); i++) {
    anchors[i].distance = anchors[i].distance + 2 * error * (randunif(rande) - 0.5);
  }
  
  solution_t solution_with_errors = MLAT::solve(anchors);

  cout << "Anchors with errors" << endl;
  print_anchors(anchors);
  cout << "Node" << endl;
  print_position(node_pos);
  cout << "Solution" << endl;
  print_solution(solution_with_errors);

  position_t mlat_single_0 = MLAT::solve_single_anchor(anchors[0], 0);
  position_t mlat_single_45 = MLAT::solve_single_anchor(anchors[0], 45);
  position_t mlat_single_117 = MLAT::solve_single_anchor(anchors[0], 117);

  double_solution_t mlat_double_0 = MLAT::solve_two_anchors(anchors[0], anchors[1]);

  cout << "Not ideal solutions:" << endl;
  cout << "Anchors with errors" << endl;
  print_anchors(anchors);
  cout << "Node" << endl;
  print_position(node_pos);
  cout << "Single anchor [0]:" << endl;
  cout << "Angle 0:" << endl;
  print_position(mlat_single_0);
  cout << "Angle 45:" << endl;
  print_position(mlat_single_45);
  cout << "Angle 117:" << endl;
  print_position(mlat_single_117);

  cout << "Two anchors [0][1]:" << endl;
  cout << "Solution 1:" << endl;
  print_position(mlat_double_0.pos1);
  cout << "Solution 2:" << endl;
  print_position(mlat_double_0.pos2);
  
  vTaskDelete(NULL);
}

extern "C" void app_main(void)
{
    xTaskCreate(mlat_test, "mlattest", 1024*40, NULL, tskIDLE_PRIORITY, NULL);
}