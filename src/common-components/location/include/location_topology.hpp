#ifndef LOCATION_TOPOLOGY_H_
#define LOCATION_TOPOLOGY_H_

#include <inttypes.h>
#include <vector>
#include <unordered_map>
#include <utility>
#include <memory>
#include "location_defs.h"
#include "interactive-mesh-framework.hpp"
#include "esp_err.h"

class LocationTopology{
    public:
        LocationTopology(std::vector<std::shared_ptr<imf::Device>> stations, uint16_t max_iters_per_step);
        ~LocationTopology();
        esp_err_t start();
        esp_err_t stop();
    private:
        std::shared_ptr<imf::Device> _this_device;
        std::unordered_map<uint32_t, std::shared_ptr<imf::Device>> _stations;
        uint16_t _max_iters_per_step;
        TaskHandle_t _xHandle = NULL;
        GVC_t *_gvc;
        Agraph_t *_g;
        std::unordered_map<uint32_t, Agnode_t*> _nodes;
        std::vector<std::pair<uint32_t, uint32_t>, Agedge_t*> _edges;
        esp_err_t uint32ToStr(uint32_t num, size_t buf_len, char  *buf);
        esp_err_t locationToPosStr(const location_local_t *location, size_t buf_len, char *buf);
        void initGraph();
        void removeNodeEdges(uint32_t id);
        void removeNode(uint32_t id);
        esp_err_t updateNodePosition(uint32_t id);
        void addNode(uint32_t id);
        void addEdge(uint32_t source, uint32_t target, uint32_t distance);
        void updateGraph();
        void updateNodePositions();
        void task();
        void singleRun();
        void taskWrapper(void* param){
            static_cast<LocationTopology *>(param)->task();
        }
}

#endif