#include "mlat_localization.hpp"
#include "mlat.hpp"
#include "esp_random.h"
#include <string>
#include <cmath>
#include <cstdint>
#include "logger.h"
#include <algorithm>

using namespace imf;
using namespace mlat;

constexpr float distance_scale = 1.0/100.0; // cm to m

static const char* TAG = "MLAT_LOC";

constexpr int pos_scale = 100;

constexpr size_t closest_anchors_limit = 5;

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
    x = (float) location.local_north / pos_scale;
    y = (float) location.local_east  / pos_scale;
}

void MlatLocalization::posToLocation(const float x, const float y, location_local_t &location){
    location.local_north = x * pos_scale;
    location.local_east  = y * pos_scale;
}

static inline float distance_2d(const position_t pos1, const position_t pos2){
  return sqrt(  pow(pos1.x - pos2.x, 2) 
              + pow(pos1.y - pos2.y, 2));
}

void MlatLocalization::tick(TickType_t diff){
    location_local_t new_location{0,0,0,0,0};
    std::vector<anchor_t> anchors;
    for(auto && [id,station] : _stations){
        location_local_t location;
        distance_log_t dist_log;
        esp_err_t err;
        
        err = station->getLocation(location);
        if(err != ESP_OK){
            LOGGER_I(TAG, "skip id %" PRIu32 ", no location", id);
            continue;
        }
        if(location.uncertainty >= std::numeric_limits<uint16_t>::max()/2){
            LOGGER_I(TAG, "skip id %" PRIu32 ", high uncertainty");
            continue;
        }

        err = station->lastDistance(dist_log);
        if(err != ESP_OK){
            LOGGER_I(TAG, "skip id %" PRIu32 ", no distance", id);
            continue;
        }

        float distance = (float)dist_log.measurement.distance_cm * distance_scale;
        float x,y;
        locationToPos(location, x, y);
        LOGGER_I(TAG, "id %" PRIu32 " distance %" PRIu32 "(%f, RSSI %" PRId8 ") pos=x%f,y%f", id, dist_log.measurement.distance_cm, 
            distance, dist_log.measurement.rssi, x, y);
        anchors.emplace_back((position_t){x,y}, distance);
    }

    LOGGER_I(TAG, "Anchors (%d):", anchors.size());
    for(auto && anchor : anchors){
        LOGGER_I(TAG, "-> x=%f,y=%f,d=%f", anchor.pos.x, anchor.pos.y, anchor.distance);
    }

    // perform multilateration according to number of anchors
    if(anchors.size() >= 3){
        // use only x closest distances (closer distances are generally more accurate)
        std::sort(anchors.begin(), anchors.end(), [](anchor_t a, anchor_t b)
                                  {
                                      return a.distance < b.distance;
                                  });
        std::vector<anchor_t> closest_anchors;
        for(auto && anchor : anchors){
            closest_anchors.push_back(anchor);
            if(closest_anchors.size() >= closest_anchors_limit){
                break;
            }
        }
        LOGGER_I(TAG, "Closest anchors (%d):", closest_anchors.size());
        for(auto && anchor : closest_anchors){
            LOGGER_I(TAG, "-> x=%f,y=%f,d=%f", anchor.pos.x, anchor.pos.y, anchor.distance);
        }
        solution_t solution = MLAT::solve(closest_anchors);
        LOGGER_I(TAG, "resulting pos x=%f,y=%f,err=%f", solution.pos.x, solution.pos.y, solution.error);
        posToLocation(solution.pos.x, solution.pos.y, new_location);
        new_location.uncertainty = (uint16_t) abs(solution.error);
    }
    else if(anchors.size() == 2){
        double_solution_t solutions = MLAT::solve_two_anchors(anchors[0], anchors[1]);
        LOGGER_I(TAG, "resulting pos x=%f,y=%f", solutions.pos1.x, solutions.pos1.y);
        posToLocation(solutions.pos1.x, solutions.pos1.y, new_location);
        new_location.uncertainty = (uint16_t) distance_2d(solutions.pos1, solutions.pos2);
    }
    else if(anchors.size() == 1){
        position_t pos = MLAT::solve_single_anchor(anchors[0], 0.0);
        LOGGER_I(TAG, "resulting pos x=%f,y=%f", pos.x, pos.y);
        posToLocation(pos.x, pos.y, new_location);
        new_location.uncertainty = 0;
    }
    else{
        new_location.local_north = 0;
        new_location.local_east  = 0;
        new_location.uncertainty = 0;
        LOGGER_I(TAG, "no anchors, setting pos to x=0,y=0");
    }
    _this_device->setLocation(new_location);
}

void MlatLocalization::task(){
    while(true){
        tick(2000 / portTICK_PERIOD_MS);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(_xHandle);
}