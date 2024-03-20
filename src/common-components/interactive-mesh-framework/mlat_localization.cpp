#include "mlat_localization.hpp"
#include "mlat.hpp"
#include "neato_esp.h"
#include "esp_random.h"
#include <string>
#include <cmath>
#include <cstdint>
#include "logger.h"

using namespace imf;
using namespace mlat;

static float distance_scale = 1/10;

static const char* TAG = "MLAT_LOC";

MlatLocalization::MlatLocalization(std::shared_ptr<Device> this_device, std::vector<std::shared_ptr<Device>> stations)
    : _this_device(this_device){
    for(size_t i = 0; i < stations.size(); i++){
        _stations.emplace(stations[i]->id, stations[i]);
    }
}
bool MlatLocalization::start(){
    auto ret = xTaskCreatePinnedToCore(taskWrapper, "MlatLocalization", 1024*20, this, tskIDLE_PRIORITY+2, &_xHandle, 1);
    if(ret != pdPASS){
        ESP_LOGE(TAG, "Could not create Location task");
        return false;
    }
    return true;
}

void MlatLocalization::stop(){
    if(_xHandle != NULL){
        vTaskDelete(_xHandle);
        _xHandle = NULL;
    }
}
void MlatLocalization::locationToPos(const location_local_t &location, float &x, float &y){
    x = (float) location.local_north / 10;
    y = (float) location.local_east  / 10;
}

void MlatLocalization::posToLocation(const float x, const float y, location_local_t &location){
    location.local_north = x * 10;
    location.local_east  = y * 10;
}

static inline float distance_2d(const position_t pos1, const position_t pos2){
  return sqrt(  pow(pos1.x - pos2.x, 2) 
              + pow(pos1.y - pos2.y, 2));
}

void MlatLocalization::singleRun(){
    location_local_t new_location{0,0,0,0,0};
    std::vector<anchor_t> anchors;
    for(auto && [id,station] : _stations){
        location_local_t location;
        uint32_t distance_cm;
        esp_err_t err;

        err = station->getLocation(location);
        if(err != ESP_OK || location.uncertainty >= std::numeric_limits<uint16_t>::max()/2){
            continue;
        }

        err = station->lastDistance(distance_cm);
        if(err != ESP_OK){
            continue;
        }
        
        float distance = (float)distance_cm * distance_scale;
        float x,y;
        locationToPos(location, x, y);
        anchors.emplace_back((position_t){x,y}, distance);
    }

    // perform multilateration according to number of anchors
    if(anchors.size() >= 3){
        solution_t solution = MLAT::solve(anchors);
        posToLocation(solution.pos.x, solution.pos.y, new_location);
        new_location.uncertainty = (uint16_t) abs(solution.error);
    }
    else if(anchors.size() == 2){
        double_solution_t solutions = MLAT::solve_two_anchors(anchors[0], anchors[1]);
        posToLocation(solutions.pos1.x, solutions.pos1.y, new_location);
        new_location.uncertainty = (uint16_t) distance_2d(solutions.pos1, solutions.pos2);
    }
    else if(anchors.size() == 1){
        position_t pos = MLAT::solve_single_anchor(anchors[0], 0.0);
        posToLocation(pos.x, pos.y, new_location);
        new_location.uncertainty = 0;
    }
    else{
        new_location.local_north = 0;
        new_location.local_east  = 0;
        new_location.uncertainty = 0;
    }
    _this_device->setLocation(new_location);
}

void MlatLocalization::task(){
    while(true){
        singleRun();
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(_xHandle);
}