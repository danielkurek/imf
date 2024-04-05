#include "graph_localization.hpp"
#include "neato_esp.h"
#include "esp_random.h"
#include <string>
#include <math.h>
#include "logger.h"

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

GraphLocalization::GraphLocalization(const std::string& mode, std::shared_ptr<Device> this_device, std::vector<std::shared_ptr<Device>> stations, uint16_t max_iters_per_step)
    : _mode(mode), _this_device(this_device), _max_iters_per_step(max_iters_per_step){
    for(size_t i = 0; i < stations.size(); i++){
        _stations.emplace(stations[i]->id, stations[i]);
    }
    _gvc = gvContext();
    // initGraph();
}

GraphLocalization::~GraphLocalization(){
    stop();
    freeGraph();
    gvFreeContext(_gvc);
}

void GraphLocalization::freeGraph(){
    if(_g){
        ESP_LOGI(TAG, "Freeing graph");
        gvFreeLayout(_gvc, _g);
        int ret = agclose(_g);
        if(ret != 0){
            ESP_LOGE(TAG, "Could not close graph! ret=%d", ret);
        }
        _g = NULL;
        return;
    }
    ESP_LOGI(TAG, "Graph could not be freed");
}

void GraphLocalization::initGraph(){
    ESP_LOGI(TAG, "Initialing new graph");
    _g = agopen(graph_name, Agundirected, 0);
    agsafeset(_g, mode_name, _mode.c_str(), "");
    agsafeset(_g, notranslate_name, "true", "");
    agsafeset(_g, inputscale_name, "72", "");
    // addNode(_this_device->id);
}

void GraphLocalization::removeNode(uint32_t id){
    Agnode_t *node = getNode(id);
    if(node){
        agdelnode(_g, node);
    }
}

esp_err_t GraphLocalization::uint32ToStr(uint32_t num, size_t buf_len, char  *buf){
    int ret = snprintf(buf, buf_len, "%" PRIu32, num);
    if(ret > 0){
        return ESP_OK;
    }
    return ESP_FAIL;
}

void GraphLocalization::locationToPos(const location_local_t &location, float &x, float &y){
    x = ((float) location.local_north * 72.0)/ 10;
    y = ((float) location.local_east  * 72.0)/ 10;
}
void GraphLocalization::posToLocation(const float x, const float y, location_local_t &location){
    location.local_north = (x / 72.0) * 10;
    location.local_east  = (y / 72.0) * 10;
}

esp_err_t GraphLocalization::locationToPosStr(const location_local_t &location, size_t buf_len, char *buf){
    float x, y;
    locationToPos(location, x, y);
    int ret = snprintf(buf, buf_len, "%f,%f", x,y);
    if(ret > 0){
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t GraphLocalization::posStrToLocation(const char* pos_str, location_local_t &location){
    float x, y;
    int ret = sscanf(pos_str, "%f,%f", &x, &y);
    if(ret != 2){
        return ESP_FAIL;
    }
    posToLocation(x, y, location);
    return ESP_OK;
}

esp_err_t GraphLocalization::nodeDistance(uint32_t id1, uint32_t id2, float &result){
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

    float x1,y1;
    locationToPos(loc1, x1, y1);

    float x2,y2;
    locationToPos(loc2, x2, y2);
    
    result = sqrt(pow(x2-x1, 2) + pow(y2-y1, 2));
    return ESP_OK;
}

std::shared_ptr<imf::Device> GraphLocalization::getDevice(uint32_t id){
    if(_this_device && id == _this_device->id){
        return _this_device;
    }
    auto iter = _stations.find(id);
    if(iter == _stations.end()){
        return nullptr;
    }
    return iter->second;
}

esp_err_t GraphLocalization::updateNodePosition(uint32_t id){
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

    Agnode_t *node = getNode(id);
    if(node == NULL){
        return ESP_FAIL;
    }
    agsafeset(node, pos_name, pos_str, "");
    if(_this_device && id != _this_device->id){
        agsafeset(node, pin_name, "true", "");
    }

    // TODO: update all edges
    return ESP_OK;
}

Agnode_t *GraphLocalization::getNode(uint32_t id, bool create){
    char id_str[10+1];
    uint32ToStr(id, 10+1, id_str);

    Agnode_t *node = agnode(_g, id_str, create ? 1 : 0);
    if(node == NULL){
        LOGGER_E(TAG, "agnode returned NULL instead of node! node_id=%" PRIu32, id);
        return NULL;
    }
    return node;
}

Agnode_t *GraphLocalization::addNode(uint32_t id){
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

    // create node
    Agnode_t *node = getNode(id, true);
    if(node == NULL){
        LOGGER_E(TAG, "agnode returned NULL instead of node! node_id=%" PRIu32, id);
        return NULL;
    }
    ESP_LOGI(TAG, "Adding node %" PRIu32 " pointer=%p", id, (void *)node);

    // set attributes

    // assume that 0,0 position is unassigned (in future use the uncertainty value)
    if(location.uncertainty < UINT16_MAX){
        agsafeset(node, pos_name, pos_str, "");
    }
    if(!_this_device || (_this_device && id != _this_device->id)){
        agsafeset(node, pin_name, "true", "");
    }
    return node;
}

Agedge_t *GraphLocalization::getEdge(uint32_t source, uint32_t target, bool create){
    // normalize input
    if(source > target){
        uint32_t tmp = source;
        source = target;
        target = tmp;   
    }

    Agnode_t *source_node = getNode(source);
    Agnode_t *target_node = getNode(target);
    ESP_LOGI(TAG, "Creating edge from=%" PRIu32 " (pointer=%p) to=%" PRIu32 " (pointer=%p)", source, (void *) source_node, target, (void *) target_node);
    if(source_node == NULL || target_node == NULL){
        ESP_LOGE(TAG, "While creating edge source node(%p) or target node(%p) is NULL", (void *) source_node, (void *) target_node);
        return NULL;
    }
    Agedge_t *e = agedge(_g, source_node, target_node, 0, create ? 1 : 0);
    
    if(e == NULL){
        LOGGER_E(TAG, "agedge returned NULL instead of edge! source_id=%" PRIu32" target_id=%" PRIu32, source, target);
        return e;
    }

    return e;
}

Agedge_t *GraphLocalization::addEdge(uint32_t source, uint32_t target, float distance){
    std::string dist_str = std::to_string(distance);
    
    Agedge_t *e = getEdge(source, target, true);
    if(e == NULL){
        ESP_LOGE(TAG, "getEdge returned null for source=%" PRIu32 " target=%" PRIu32, source, target);
        return NULL;
    }
    ESP_LOGI(TAG, "Setting edge (pointer=%p) from=%" PRIu32 " to=%" PRIu32 " with len=%s", (void *)e, source, target, dist_str.c_str());
    agsafeset(e, len_name, dist_str.c_str(), "");
    return e;
}

void GraphLocalization::populateGraph(){
    ESP_LOGI(TAG, "Updating graph");
    constexpr TickType_t delay = 200 / portTICK_PERIOD_MS;
    std::vector<uint32_t> unknown_dist_nodes;
    std::vector<uint32_t> local_nodes;
    if(_this_device){
        addNode(_this_device->id);
        local_nodes.push_back(_this_device->id);
    } else{
        ESP_LOGE(TAG, "this device is not initialized");
        return;
    }
    ESP_LOGI(TAG, "Adding reachable stations");
    for(auto iter = _stations.begin(); iter != _stations.end(); ++iter){
        auto id = iter->first;
        auto device = iter->second;
        uint32_t distance_cm;
        esp_err_t err = device->lastDistance(distance_cm);
        if(err != ESP_OK){
            unknown_dist_nodes.push_back(id);
            continue;
        }
        local_nodes.push_back(id);
        addNode(id);
        addEdge(_this_device->id, id, ((float) distance_cm) / 100.0);
        vTaskDelay(delay);
    }
    ESP_LOGI(TAG, "Adding unreachable stations");
    for(const auto &local_id : local_nodes){
        for(const auto &other_id : unknown_dist_nodes){
            float distance;
            esp_err_t err = nodeDistance(local_id, other_id, distance);
            vTaskDelay(delay);
            if(err != ESP_OK){
                // remove something?
                continue;
            }
            // ensure nodes exist
            if(getNode(local_id) == NULL){
                addNode(local_id);
                vTaskDelay(delay);
            }
            if(getNode(other_id) == NULL){
                addNode(other_id);
                vTaskDelay(delay);
            }

            // limit maximum distance
            if(distance > 100){
                distance = 100;
            } else if(distance < -100){
                distance = -100;
            }

            addEdge(local_id, other_id, distance);
        }
    }
}

void GraphLocalization::saveNodePosition(uint32_t node_id){
    // auto iter = _nodes.find(node_id);
    // if(iter == _nodes.end()){
    //     return;
    // }
    Agnode_t *node = getNode(node_id);
    if(node == NULL){
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

void GraphLocalization::tick(TickType_t diff){
    heap_caps_print_heap_info(MALLOC_CAP_8BIT);
    ESP_LOGI(TAG, "START StackHighWaterMark=%d, heapfree=%d, heapminfree=%d, heapmaxfree=%d", uxTaskGetStackHighWaterMark(NULL), heap_caps_get_free_size(MALLOC_CAP_8BIT), heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT), heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    initGraph();
    populateGraph();
    int ret;
    ret = neato_esp_layout(_gvc, _g);
    if(ret != 0){
        ESP_LOGE(TAG, "Could not perform layout! ret=%d", ret);
    }
    ret = neato_esp_render(_gvc, _g, stdout); // only for debugging
    if(ret != 0){
        ESP_LOGE(TAG, "Could not perform render! ret=%d", ret);
    }
    saveNodePosition(_this_device->id);
    freeGraph();
    ESP_LOGI(TAG, "END StackHighWaterMark=%d, heapfree=%d, heapminfree=%d, heapmaxfree=%d", uxTaskGetStackHighWaterMark(NULL), heap_caps_get_free_size(MALLOC_CAP_8BIT), heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT), heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}

void GraphLocalization::task(){
    // TODO: stopping condition? - detect stagnation (nodes are not moving much)
    while(true){
        tick(500 / portTICK_PERIOD_MS);
        // uint32_t rnd = esp_random() / 1000000; // 0-4294
        // vTaskDelay(1000 + rnd); // random delay 1000 - 5294 ticks
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}

bool GraphLocalization::start(){
    auto ret = xTaskCreatePinnedToCore(taskWrapper, "GraphLocalization", 1024*20, this, tskIDLE_PRIORITY+2, &_xHandle, 1);
    if(ret != pdPASS){
        ESP_LOGE(TAG, "Could not create Location task");
        return false;
    }
    return true;
}

void GraphLocalization::stop(){
    if(_xHandle != NULL){
        vTaskDelete(_xHandle);
        _xHandle = NULL;
    }
}