#include "location_topology.hpp"
#include "neato_esp.h"
#include "esp_random.h"
#include <string>
#include <math.h>

using namespace imf;

static const char* TAG = "LOC_TOP";

// TODO: verify that methods does not modify these
// agget - does not modify or free the char*
static char graph_name[] = "g";
static char mode_name[] = "mode";
static char pos_name[] = "pos";
static char pin_name[] = "pin";
static char notranslate_name[] = "notranslate";
static char len_name[] = "len";
static char inputscale_name[] = "inputscale";

// pair will be number "xy" (concatenated)
static inline uint64_t make_pair(uint32_t x, uint32_t y){
    uint64_t pair = y;
    pair |= (uint64_t) x << 32;
    return pair;
}

static inline void extract_pair(uint64_t pair, uint32_t *x, uint32_t *y){
    (*x) = pair >> 32;
    (*y) = pair & UINT32_MAX;
}

LocationTopology::LocationTopology(const std::string& mode, std::shared_ptr<Device> this_device, std::vector<std::shared_ptr<Device>> stations, uint16_t max_iters_per_step)
    : _mode(mode), _this_device(this_device), _max_iters_per_step(max_iters_per_step){
    for(size_t i = 0; i < stations.size(); i++){
        _stations.emplace(stations[i]->id, stations[i]);
    }
    _gvc = gvContext();
    // initGraph();
}

LocationTopology::~LocationTopology(){
    stop();
    freeGraph();
    gvFreeContext(_gvc);
}

void LocationTopology::freeGraph(){
    if(_g){
        gvFreeLayout(_gvc, _g);
        agclose(_g);
    }
    _nodes.clear();
    _edges.clear();
}

void LocationTopology::initGraph(){
    _g = agopen(graph_name, Agundirected, 0);
    agsafeset(_g, mode_name, _mode.c_str(), "");
    agsafeset(_g, notranslate_name, "true", "");
    agsafeset(_g, inputscale_name, "72", "");
    // addNode(_this_device->id);
}

void LocationTopology::removeNodeEdges(uint32_t id){
    auto iter = _edges.begin();
    while(iter != _edges.end()){
        uint32_t id1, id2;
        extract_pair(iter->first, &id1, &id2);
        if(id1 == id || id2 == id){
            Agedge_t *edge = addEdge(id1, id2, 0.0);
            if(edge){
                agdeledge(_g, edge);
            }
            iter = _edges.erase(iter);
            continue;
        }
        ++iter;
    }
}

void LocationTopology::removeNode(uint32_t id){
    removeNodeEdges(id);
    Agnode_t *node = addNode(id);
    if(node){
        agdelnode(_g, node);
    }
    auto iter = _nodes.find(id);
    if(iter == _nodes.end()){
        return;
    }
    _nodes.erase(iter);
}

esp_err_t LocationTopology::uint32ToStr(uint32_t num, size_t buf_len, char  *buf){
    int ret = snprintf(buf, buf_len, "%" PRIu32, num);
    if(ret > 0){
        return ESP_OK;
    }
    return ESP_FAIL;
}

void LocationTopology::locationToPos(const location_local_t &location, float &x, float &y){
    x = ((float) location.local_north * 72)/ 10;
    y = ((float) location.local_east  * 72)/ 10;
}
void LocationTopology::posToLocation(const float x, const float y, location_local_t &location){
    location.local_north = (x / 72) * 10;
    location.local_east  = (y / 72) * 10;
}

esp_err_t LocationTopology::locationToPosStr(const location_local_t &location, size_t buf_len, char *buf){
    float x, y;
    locationToPos(location, x, y);
    int ret = snprintf(buf, buf_len, "%f,%f", x,y);
    if(ret > 0){
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t LocationTopology::posStrToLocation(const char* pos_str, location_local_t &location){
    float x, y;
    int ret = sscanf(pos_str, "%f,%f", &x, &y);
    if(ret != 2){
        return ESP_FAIL;
    }
    posToLocation(x, y, location);
    return ESP_OK;
}

esp_err_t LocationTopology::nodeDistance(uint32_t id1, uint32_t id2, float &result){
    auto device1 = getDevice(id1);
    auto device2 = getDevice(id2);
    
    if(!device1 || !device2) return ESP_FAIL;

    esp_err_t err;
    location_local_t loc1 {0,0,0,0,0};
    location_local_t loc2 {0,0,0,0,0};
    err = device1->getLocation(loc1);
    if(err != ESP_OK){
        LOGGER_E(TAG, "Could not get location for device with id %" PRIu32, id1);
        return err;
    }
    err = device2->getLocation(loc2);
    if(err != ESP_OK){
        LOGGER_E(TAG, "Could not get location for device with id %" PRIu32, id2);
        return err;
    }

    float dist_north = loc1.local_north + loc2.local_north;
    float dist_east = loc1.local_east + loc2.local_east;
    result = sqrt(pow(dist_north, 2) + pow(dist_east, 2));
    return ESP_OK;
}

std::shared_ptr<imf::Device> LocationTopology::getDevice(uint32_t id){
    if(_this_device && id == _this_device->id){
        return _this_device;
    }
    auto iter = _stations.find(id);
    if(iter == _stations.end()){
        return nullptr;
    }
    return iter->second;
}

esp_err_t LocationTopology::updateNodePosition(uint32_t id){
    if(!_nodes.contains(id)){
        return ESP_FAIL;
    }
    esp_err_t err;
    std::shared_ptr<Device> device = getDevice(id);
    if(!device){
        return ESP_FAIL;
    }
    location_local_t location {0,0,0,0,0};
    err = device->getLocation(location);
    if(err != ESP_OK){
        LOGGER_E(TAG, "Could not get location for node %" PRIu32, id);
        return err;
    }
    char pos_str[64];
    err = locationToPosStr(location, 64, pos_str);
    if(err != ESP_OK){
        LOGGER_E(TAG, "Could not convert location to position string for node %" PRIu32, id);
        return err;
    }
    // TODO: check return code
    if(_nodes[id] == NULL){
        LOGGER_E(TAG, "node with id %" PRIu32 " is NULL", id);
        // return ESP_FAIL;
    }
    Agnode_t *node = addNode(id);
    if(!node){
        return ESP_FAIL;
    }
    agsafeset(node, pos_name, pos_str, "");
    if(id != _this_device->id){
        agsafeset(node, pin_name, "true", "");
    }

    // TODO: update all edges
    return ESP_OK;
}

Agnode_t *LocationTopology::addNode(uint32_t id){
    // if(_nodes.contains(id)){
    //     return NULL;
    // }

    // get location attribute
    location_local_t location {0,0,0,0,0};
    esp_err_t err;
    std::shared_ptr<Device> device = getDevice(id);
    if(!device){
        return NULL;
    }
    err = device->getLocation(location);
    if(err != ESP_OK){
        LOGGER_E(TAG, "Could not get location for node %" PRIu32, id);
        return NULL;
    }
    char pos_str[64];
    err = locationToPosStr(location, 64, pos_str);
    if(err != ESP_OK){
        LOGGER_E(TAG, "Could not convert location to position string for node %" PRIu32, id);
        return NULL;
    }

    // create node and set attributes
    char id_str[10+1];
    uint32ToStr(id, 10+1, id_str);

    Agnode_t *node = agnode(_g, id_str, 1);
    if(node == NULL){
        LOGGER_E(TAG, "agnode returned NULL instead of node! node_id=%" PRIu32, id);
        return NULL;
    }
    ESP_LOGI(TAG, "Adding node %" PRIu32 " pointer=%p", id, (void *)node);
    _nodes.emplace(id, node);
    // updateNodePosition(id);

    agsafeset(node, pos_name, pos_str, "");
    if(id != _this_device->id){
        agsafeset(node, pin_name, "true", "");
    }
    return node;
}

Agedge_t *LocationTopology::addEdge(uint32_t source, uint32_t target, float distance){
    // if(!_nodes.contains(source) || !_nodes.contains(target)){
    //     return NULL;
    // }
    uint64_t edge;
    if(source > target){
        uint32_t tmp = source;
        source = target;
        target = tmp;   
    }
    edge = make_pair(source, target);
    std::string dist_str = std::to_string(distance);
    // auto iter = _edges.find(edge);
    // if(iter != _edges.end()){
    //     ESP_LOGI(TAG, "Setting edge (pointer=%p) from=%" PRIu32 " to=%" PRIu32 " to len=%s", (void *)iter->second, source, target, dist_str.c_str());
    //     agsafeset(iter->second, len_name, dist_str.c_str(), "");
    //     return NULL;
    // }
    ESP_LOGI(TAG, "Creating edge from=%" PRIu32 " (pointer=%p) to=%" PRIu32 " (pointer=%p)", source, (void *) _nodes[source], target, (void *) _nodes[target]);
    Agnode_t *source_node = addNode(source);
    Agnode_t *target_node = addNode(target);
    ESP_LOGI(TAG, "Creating edge from=%" PRIu32 " (pointer=%p) to=%" PRIu32 " (pointer=%p)", source, (void *) source_node, target, (void *) target_node);
    if(source_node == NULL || target_node == NULL){
        ESP_LOGE(TAG, "While creating edge source node(%p) or target node(%p) is NULL", (void *) source_node, (void *) target_node);
        return NULL;
    }
    Agedge_t *e = agedge(_g, source_node, target_node, 0, 1);
    if(e == NULL){
        ESP_LOGE(TAG, "agedge returned null for source=%" PRIu32 " target=%" PRIu32, source, target);
        return NULL;
    }
    ESP_LOGI(TAG, "Setting edge (pointer=%p) from=%" PRIu32 " to=%" PRIu32 " to len=%s", (void *)e, source, target, dist_str.c_str());
    agsafeset(e, len_name, dist_str.c_str(), "");
    ESP_LOGI(TAG, "Saving edge (pointer=%p) from=%" PRIu32 " (pointer=%p) to=%" PRIu32 " (pointer=%p)", (void *)e, source, (void *) _nodes[source], target, (void *) _nodes[target]);
    _edges.emplace(edge, e);
    return e;
}

void LocationTopology::updateGraph(){
    std::vector<uint32_t> unknown_dist_nodes;
    std::vector<uint32_t> neighbor_nodes;
    auto find = _nodes.find(_this_device->id);
    if(find == _nodes.end()){
        addNode(_this_device->id);
    }
    for(auto iter = _stations.begin(); iter != _stations.end(); ++iter){
        auto id = iter->first;
        auto device = iter->second;
        uint32_t distance_cm;
        esp_err_t err = device->lastDistance(distance_cm);
        if(err != ESP_OK){
            unknown_dist_nodes.push_back(id);
            removeNodeEdges(id);
            continue;
        }
        neighbor_nodes.push_back(id);
        auto find = _nodes.find(id);
        if(find == _nodes.end()){
            addNode(id);
        }
        addEdge(_this_device->id, id, distance_cm);
    }
    for(const auto &neighbor_id : neighbor_nodes){
        for(const auto &other_id : unknown_dist_nodes){
            float distance;
            esp_err_t err = nodeDistance(neighbor_id, other_id, distance);
            if(err != ESP_OK){
                // remove something?
                continue;
            }
            // ensure nodes exist
            addNode(neighbor_id);
            addNode(other_id);

            addEdge(neighbor_id, other_id, distance);
        }
    }
}

void LocationTopology::updateNodePositions(){
    for(const auto &iter : _nodes){
        updateNodePosition(iter.first);
    }
}

void LocationTopology::saveNodePosition(uint32_t node_id){
    // auto iter = _nodes.find(node_id);
    // if(iter == _nodes.end()){
    //     return;
    // }
    Agnode_t *node = addNode(node_id);
    if(!node){
        LOGGER_E(TAG, "Could not get node with id %" PRIu32, node_id);
        return;
    }
    char *pos_str = agget(node, pos_name);
    if(pos_str == 0){
        LOGGER_E(TAG, "Could not get position argument for node with id %" PRIu32, node_id);
        return;
    }
    location_local_t location {0,0,0,0,0};
    posStrToLocation(pos_str, location);
    auto device = getDevice(node_id);
    if(device){
        device->setLocation(location);
    }else {
        LOGGER_E(TAG, "Could not find station with id %" PRIu32, node_id);
    }
}

void LocationTopology::saveNodePositions(){
    for(const auto &iter : _nodes){
        saveNodePosition(iter.first);
    }
}

void LocationTopology::singleRun(){
    initGraph();
    updateGraph();
    neato_esp_layout(_gvc, _g);
    neato_esp_render(_gvc, _g, stdout); // only for debugging
    saveNodePosition(_this_device->id);
    freeGraph();
}

void LocationTopology::task(){
    // TODO: stopping condition? - detect stagnation (nodes are not moving much)
    while(true){
        singleRun();
        uint32_t rnd = esp_random() / 1000000; // 0-4294
        vTaskDelay(1000 + rnd); // random delay 1000 - 5294 ticks
    }
}

esp_err_t LocationTopology::start(){
    return xTaskCreatePinnedToCore(taskWrapper, "LocationTopology", 1024*32, this, tskIDLE_PRIORITY+2, &_xHandle, 1);
}

void LocationTopology::stop(){
    if(_xHandle != NULL){
        vTaskDelete(_xHandle);
        _xHandle = NULL;
    }
}