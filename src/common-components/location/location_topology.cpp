#include "location_topology.hpp"
#include "neato_esp.h"
#include "esp_random.h"

using namespace imf;

static const char* TAG = "LOC_TOP";

// pair will be number "xy" (concatenated)
static inline uint64_t make_pair(uint32_t x, uint32_t y){
    uint64_t pair = y;
    pair |= x << 32;
    return pair;
}

static inline void extract_pair(uint64_t pair, uint32_t *x, uint32_t *y){
    (*x) = pair >> 32;
    (*y) = pair & UINT32_MAX;
}

LocationTopology::LocationTopology(std::shared_ptr<Device> this_device, std::vector<std::shared_ptr<Device>> stations, uint16_t max_iters_per_step)
    : _this_device(this_device), _max_iters_per_step(max_iters_per_step){
    for(size_t i = 0; i < stations.size(); i++){
        _stations.emplace(stations[i]->id, stations[i]);
    }
    initGraph();
}

LocationTopology::~LocationTopology(){
    stop();
    gvFreeLayout(_gvc, _g);
    agclose(_g);
    _nodes.clear();
    _edges.clear();
    gvFreeContext(_gvc);
}

void LocationTopology::initGraph(){
    _g = agopen("g", Agundirected, 0);
    agset(_g, "mode", "sgd");
    agset(_g, "notranslate", "true");
    agset(_g, "inputscale", "72");
    addNode(_this_device->id);
}

void LocationTopology::removeNodeEdges(uint32_t id){
    auto iter = _edges.begin();
    while(iter != _edges.end()){
        uint32_t id1, id2;
        extract_pair(iter->first, &id1, &id2);
        if(id1 == id || id2 == id){
            agdeledge(_g, iter->second);
            iter = _edges.erase(iter);
            continue;
        }
        ++iter;
    }
}

void LocationTopology::removeNode(uint32_t id){
    auto iter = _nodes.find(id);
    if(iter == _nodes.end()){
        return;
    }
    
    removeNodeEdges(id);

    agdelnode(_g, iter->second);
    _nodes.erase(iter);
}

esp_err_t LocationTopology::uint32ToStr(uint32_t num, size_t buf_len, char  *buf){
    int ret = snprintf(buf, buf_len, "%" PRIu32, num);
    if(ret > 0){
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t LocationTopology::locationToPosStr(const location_local_t *location, size_t buf_len, char *buf){
    int ret = snprintf(buf, buf_len, "%" PRId16 ",%" PRId16, location->local_north, location->local_east);
    if(ret > 0){
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t LocationTopology::updateNodePosition(uint32_t id){
    if(!_nodes.contains(id)){
        return ESP_FAIL;
    }
    esp_err_t err;
    std::shared_ptr<Device> device;
    if(_this_device->id == id){
        device = _this_device;
    } else{
        auto search = _stations.find(id);
        if(search == _stations.end()){
            return ESP_FAIL;
        } 
        device = search->second;
    }
    location_local_t location;
    err = device->getLocation(&location);
    if(err != ESP_OK){
        LOGGER_E(TAG, "Could not get location for node %" PRIu32, id);
        return err;
    }
    char pos_str[(7*2)+1+1];
    err = locationToPosStr(&location, (7*2)+1+1, pos_str);
    if(err != ESP_OK){
        LOGGER_E(TAG, "Could not convert location to position string for node %" PRIu32, id);
        return err;
    }
    // TODO: check return code
    agset(_nodes[id], "pos", pos_str);
    return ESP_OK;
}

void LocationTopology::addNode(uint32_t id){
    if(_nodes.contains(id)){
        return;
    }
    char id_str[10+1];
    uint32ToStr(id, 10+1, id_str);

    Agnode_t *node = agnode(_g, id_str, 1);
    _nodes.emplace(id, node);
    updateNodePosition(id);
}

void LocationTopology::addEdge(uint32_t source, uint32_t target, uint32_t distance){
    if(!_nodes.contains(source) || !_nodes.contains(target)){
        return;
    }
    uint64_t edge;
    if(source < target){
        edge = make_pair(source, target);
    } else{
        edge = make_pair(target, source);
    }
    char distance_str[10+1];
    uint32ToStr(distance, 10+1, distance_str);
    auto iter = _edges.find(edge);
    if(iter != _edges.end()){
        agset(iter->second, "len", distance_str);
    }
    Agedge_t *e = agedge(_g, _nodes[source], _nodes[target], 0, 1);
    agset(e, "len", distance_str);
    _edges.emplace(edge, e);
}

void LocationTopology::updateGraph(){
    for(auto iter = _stations.begin(); iter != _stations.end(); ++iter){
        auto id = iter->first;
        auto device = iter->second;
        uint32_t distance_cm;
        esp_err_t err = device->distance(&distance_cm);
        if(err != ESP_OK){
            removeNode(id);
            continue;
        }
        auto find = _nodes.find(id);
        if(find == _nodes.end()){
            addNode(id);
        }
        addEdge(_this_device->id, id, distance_cm);
    }
}

void LocationTopology::updateNodePositions(){
    for(const auto &iter : _nodes){
        updateNodePosition(iter.first);
    }
}

void LocationTopology::setNodePositions(){

}

void LocationTopology::singleRun(){
    updateGraph();
    neato_esp_layout(_gvc, _g);
    neato_esp_render(_gvc, _g, stdout);
    setNodePositions();
}

void LocationTopology::task(){
    while(true){
        singleRun();
        uint32_t rnd = esp_random() / 1000000; // 0-4294
        vTaskDelay(1000 + rnd);
    }
}

esp_err_t LocationTopology::start(){
    return xTaskCreate(taskWrapper, "LocationTopology", 1024*8, this, configMAX_PRIORITIES-1, &_xHandle);
}

void LocationTopology::stop(){
    return vTaskDelete(_xHandle);
}