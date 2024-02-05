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

// need forward declaration because of circular dependency
namespace imf{
    class LocationTopology;
}

#include "interactive-mesh-framework.hpp"

namespace imf{
    class LocationTopology{
        public:
            LocationTopology(const std::string &mode, std::shared_ptr<imf::Device> this_device, std::vector<std::shared_ptr<imf::Device>> stations, uint16_t max_iters_per_step);
            ~LocationTopology();
            esp_err_t start();
            void stop();
        private:
            std::string _mode;
            std::shared_ptr<imf::Device> _this_device;
            std::unordered_map<uint32_t, std::shared_ptr<imf::Device>> _stations;
            uint16_t _max_iters_per_step;
            TaskHandle_t _xHandle = NULL;
            GVC_t *_gvc;
            Agraph_t *_g;
            std::unordered_map<uint32_t, Agnode_t*> _nodes;
            std::unordered_map<uint64_t, Agedge_t*> _edges;
            esp_err_t uint32ToStr(uint32_t num, size_t buf_len, char  *buf);
            esp_err_t locationToPosStr(const location_local_t *location, size_t buf_len, char *buf);
            esp_err_t posStrToLocation(const char* pos_str, location_local_t *location);
            void initGraph();
            void removeNodeEdges(uint32_t id);
            void removeNode(uint32_t id);
            esp_err_t updateNodePosition(uint32_t id);
            void addNode(uint32_t id);
            void addEdge(uint32_t source, uint32_t target, uint32_t distance);
            void updateGraph();
            void updateNodePositions();
            void saveNodePositions();
            void task();
            void singleRun();
            static void taskWrapper(void* param){
                static_cast<LocationTopology *>(param)->task();
            }
    };
}
#endif