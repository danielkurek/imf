#ifndef LOCATION_TOPOLOGY_H_
#define LOCATION_TOPOLOGY_H_

#include <inttypes.h>
#include <vector>
#include <unordered_map>
#include <utility>
#include <memory>
#include "location_defs.h"
#include "esp_err.h"

#include "gvc.h"
#include "cgraph.h"

#include "imf-device.hpp"
#include "localization.hpp"

namespace imf{
    class GraphLocalization : public Localization{
        public:
            GraphLocalization(const std::string &mode, std::shared_ptr<imf::Device> this_device, std::vector<std::shared_ptr<imf::Device>> stations, uint16_t max_iters_per_step);
            ~GraphLocalization();
            bool start();
            void stop();
            void tick(TickType_t diff);
            static void locationToPos(const location_local_t &location, float &x, float &y);
            static void posToLocation(const float x, const float y, location_local_t &location);
        private:
            std::string _mode;
            std::shared_ptr<imf::Device> _this_device;
            std::unordered_map<uint32_t, std::shared_ptr<imf::Device>> _stations;
            uint16_t _max_iters_per_step;
            TaskHandle_t _xHandle = NULL;
            GVC_t *_gvc = NULL;
            Agraph_t *_g = NULL;
            esp_err_t uint32ToStr(uint32_t num, size_t buf_len, char  *buf);
            esp_err_t locationToPosStr(const location_local_t &location, size_t buf_len, char *buf);
            esp_err_t posStrToLocation(const char* pos_str, location_local_t &location);
            esp_err_t nodeDistance(uint32_t id1, uint32_t id2, float &distance);
            std::shared_ptr<imf::Device> getDevice(uint32_t id);
            void initGraph();
            void freeGraph();
            void removeNode(uint32_t id);
            Agnode_t *getNode(uint32_t id, bool create = false);
            Agnode_t *addNode(uint32_t id);
            Agedge_t *getEdge(uint32_t source, uint32_t target, bool create = false);
            Agedge_t *addEdge(uint32_t source, uint32_t target, float distance);
            void populateGraph();
            esp_err_t updateNodePosition(uint32_t id);
            void saveNodePosition(uint32_t node_id);
            void task();
            static void taskWrapper(void* param){
                static_cast<GraphLocalization *>(param)->task();
            }
    };
}
#endif