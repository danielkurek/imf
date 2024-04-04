#ifndef MLAT_LOCALIZATION_H_
#define MLAT_LOCALIZATION_H_

#include <inttypes.h>
#include <vector>
#include <unordered_map>
#include <utility>
#include <memory>
#include "location_defs.h"
#include "esp_err.h"

#include "imf-device.hpp"
#include "localization.hpp"

namespace imf{
    class MlatLocalization : public Localization{
        public:
            MlatLocalization(std::shared_ptr<imf::Device> this_device, std::vector<std::shared_ptr<imf::Device>> stations);
            bool start();
            void stop();
            void singleRun();
            static void locationToPos(const location_local_t &location, float &x, float &y);
            static void posToLocation(const float x, const float y, location_local_t &location);
        private:
            std::shared_ptr<imf::Device> _this_device;
            std::unordered_map<uint32_t, std::shared_ptr<imf::Device>> _stations;
            TaskHandle_t _xHandle = NULL;
            esp_err_t nodeDistance(uint32_t id1, uint32_t id2, float &distance);
            void saveNodePosition(uint32_t node_id);
            void task();
            static void taskWrapper(void* param){
                static_cast<MlatLocalization *>(param)->task();
            }
    };
}
#endif