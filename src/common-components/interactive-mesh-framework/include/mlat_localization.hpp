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
            /**
             * @brief Construct a new Mlat Localization object
             * 
             * @param this_device local device
             * @param stations all possible stations which can be used as anchors
             */
            MlatLocalization(std::shared_ptr<imf::Device> this_device, std::vector<std::shared_ptr<imf::Device>> stations);
            /**
             * @copydoc Localization::start
             */
            bool start();
            /**
             * @copydoc Localization::stop
             */
            void stop();
            /**
             * @copydoc Localization::tick
             */
            void tick(TickType_t diff);

            /**
             * @brief Convert location to x,y coordinate
             * 
             * @param[in] location input location
             * @param[out] x output x coordinate
             * @param[out] y output y coordinate
             */
            static void locationToPos(const location_local_t &location, float &x, float &y);
            
            /**
             * @brief Convert x,y coordinate to location
             * 
             * @param[in] x x coordinate
             * @param[in] y y coordinate
             * @param[out] location output location
             */
            static void posToLocation(const float x, const float y, location_local_t &location);
        private:
            std::shared_ptr<imf::Device> _this_device;
            std::unordered_map<uint32_t, std::shared_ptr<imf::Device>> _stations;
            TaskHandle_t _xHandle = NULL;

            /**
             * @brief Distance between devices
             * 
             * @param[in] id1 first device id
             * @param[in] id2 second device id
             * @param[out] distance resulting distance
             * @return esp_err_t ESP_OK if distance was computed
             */
            esp_err_t nodeDistance(uint32_t id1, uint32_t id2, float &distance);

            /**
             * @brief Save new location of node
             * 
             * @param node_id node id
             */
            void saveNodePosition(uint32_t node_id);

            /**
             * @brief Task for continuous localization
             */
            void task();

            /**
             * @brief Necessary wrapper for creating thread that performs object's method
             * @param param pointer to MlatLocalization (usually @p this )
             */
            static void taskWrapper(void* param){
                static_cast<MlatLocalization *>(param)->task();
            }
    };
}
#endif