#include "location_topology.hpp"
#include "neato_esp.h"
#include "esp_random.h"

using namespace imf;

static const char* TAG = "LOC_TOP";

// TODO: verify that methods does not modify these
// agget - does not modify or free the char*
static char graph_name[] = "g";
static char mode_name[] = "mode";
static char pos_name[] = "pos";
static char notranslate_name[] = "notranslate";
static char len_name[] = "len";

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
    _g = agopen(graph_name, Agundirected, 0);
    agsafeset(_g, mode_name, _mode.c_str(), "");
    agsafeset(_g, notranslate_name, "true", "");
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

esp_err_t LocationTopology::posStrToLocation(const char* pos_str, location_local_t *location){
    float x, y;
    int ret = sscanf(pos_str, "%f,%f", &x, &y);
    if(ret != 2){    
        return ESP_FAIL;
    }
    location->local_north = x * 10;
    location->local_east  = y * 10;
    return ESP_OK;
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
    agsafeset(_nodes[id], pos_name, pos_str, "");
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
        agsafeset(iter->second, len_name, distance_str, "");
    }
    Agedge_t *e = agedge(_g, _nodes[source], _nodes[target], 0, 1);
    agsafeset(e, len_name, distance_str, "");
    _edges.emplace(edge, e);
}

void LocationTopology::updateGraph(){
    for(auto iter = _stations.begin(); iter != _stations.end(); ++iter){
        auto id = iter->first;
        auto device = iter->second;
        uint32_t distance_cm;
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

void LocationTopology::saveNodePositions(){
    for(const auto &iter : _nodes){
        char *pos_str = agget(iter.second, pos_name);
        if(pos_str == 0){
            LOGGER_E(TAG, "Could not get position argument for node with id %" PRIu32, iter.first);
            continue;
        }
        location_local_t location {};
        posStrToLocation(pos_str, &location);
        if(iter.first == _this_device->id){
            _this_device->setLocation(&location);
        } else{
            auto search = _stations.find(iter.first);
            if(search == _stations.end()){
                LOGGER_E(TAG, "Could not find station with id %" PRIu32, iter.first);
                continue;
            }
            search->second->setLocation(&location);
        }
    }
}

void LocationTopology::singleRun(){
    updateGraph();
    neato_esp_layout(_gvc, _g);
    neato_esp_render(_gvc, _g, stdout);
    saveNodePositions();
}

void LocationTopology::task(){
    while(true){
        singleRun();
        uint32_t rnd = esp_random() / 1000000; // 0-4294
        vTaskDelay(1000 + rnd); // random delay 1000 - 5294 ticks
    }
}

esp_err_t LocationTopology::start(){
}

void LocationTopology::stop(){
    vTaskDelete(_xHandle);
}