#include <stdio.h>
#include <inttypes.h>
#include "esp_log.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "esp_check.h"
#include "esp_event.h"
#include "esp_random.h"

#include "interactive-mesh-framework.hpp"
#include "location_topology.hpp"
#include "driver/uart.h"

#include <vector>
#include <memory>
#include <sstream>

#ifdef CONFIG_IDF_TARGET_ESP32C3
#define CONFIG_BUTTON GPIO_NUM_9
#else
#define CONFIG_BUTTON GPIO_NUM_0
#endif

static const char *TAG = "APP";

using namespace imf;

static std::vector<std::shared_ptr<Device>> devices;

static const std::vector<button_gpio_config_t> buttons = {
    {.gpio_num = GPIO_NUM_46, .active_level = 1},
    {.gpio_num = GPIO_NUM_47, .active_level = 1},
    {.gpio_num = GPIO_NUM_48, .active_level = 1},
    {.gpio_num = GPIO_NUM_45, .active_level = 1},
};

typedef struct {
    std::string mac;
    uint16_t ble_addr;
    uint16_t wifi_channel;
} device_conf_t;

size_t random_number(size_t max){
    uint32_t rnd = esp_random();
    return rnd % max;
}

extern "C" void button_cb(uint8_t button_num){
    ESP_LOGI(TAG, "Button no. %d was pressed", button_num);
    if(button_num == 0){
        // if(s_imf){
        //     LOGGER_I(TAG, "Starting location topology");
        //     s_imf->startLocationTopology();
        // }
    } else if(button_num == 1){
        int16_t level = 0;
        if(Device::this_device && ESP_OK != Device::this_device->getLevel(level)){
            level = 0;
        }
        // if(ESP_OK != Device::setLevelAll(level+1)){
        //     LOGGER_E(TAG, "Could not set level!");
        // }
        if(ESP_OK != Device::setLevelAll(level+1)){
            LOGGER_E(TAG, "Could not set level!");
        }
    }
}

std::string deviceInfo(std::shared_ptr<Device> device){
    constexpr TickType_t delay = 200 / portTICK_PERIOD_MS;
    if(!device){
        return "Error: Devise is null!";
    }
    std::stringstream msg;
    msg << "Device (id=" << device->id << "):\n"
        << "-----------\n";
    rgb_t rgb {0,0,0};
    if(ESP_OK == device->getRgb(rgb)){
        msg << "rgb:";
        msg << " R=" << std::to_string(rgb.red);
        msg << " G=" << std::to_string(rgb.green);
        msg << " B=" << std::to_string(rgb.blue);
        msg << "\n";
    } else{
        msg << "rgb: Error fetching value!\n";
    }
    vTaskDelay(delay);
    location_local_t location {0,0,0,0,0};
    if(ESP_OK == device->getLocation(location)){
        float x,y;
        LocationTopology::locationToPos(location, x, y);
        msg << "pos:";
        msg << " x=" << std::to_string(x);
        msg << " y=" << std::to_string(y);
        msg << "(loc:";
        msg << " north="  << std::to_string(location.local_north);
        msg << " east="   << std::to_string(location.local_east);
        msg << " alt="    << std::to_string(location.local_altitude);
        msg << " floor="  << std::to_string(location.floor_number);
        msg << " uncert=" << std::to_string(location.uncertainty);
        msg << ")\n";
    } else{
        msg << "loc: Error fetching value!\n";
    }
    vTaskDelay(delay);
    int16_t level = 0;
    if(ESP_OK == device->getLevel(level)){
        msg << "lvl: " << std::to_string(level) << "\n";
    } else{
        msg << "lvl: Error fetching value!\n";
    }
    vTaskDelay(delay);
    return msg.str();
}

void task(void *data){
    while(true){
        std::stringstream ss;
        for(auto&& device : devices){
            ss << deviceInfo(device) << "\n";
        }
        std::string dump = ss.str();
        ESP_LOGI(TAG, "Devices dump:\n=========================\n%s", dump.c_str());
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

// start configuration mode if conditions are met
// bool web_config(){
//     gpio_config_t config;

//     config.intr_type    = GPIO_INTR_DISABLE;
//     config.mode         = GPIO_MODE_INPUT;
//     config.pin_bit_mask = (1ull << CONFIG_BUTTON);
//     config.pull_down_en = GPIO_PULLDOWN_DISABLE;
//     config.pull_up_en   = GPIO_PULLUP_ENABLE;

//     gpio_config(&config);

//     vTaskDelay(1000 / portTICK_PERIOD_MS);

//     if(gpio_get_level(CONFIG_BUTTON) == 0){
//         LOGGER_I(TAG, "Booting to web config");
//         s_imf->startWebConfig();
//         return true;
//     }
//     return false;
// }

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Startup!");
    // esp_err_t err;

    // if(web_config()){
    //     return;
    // }

    ESP_LOGI(TAG, "add devices");
    const device_conf_t devices_confs[] = {
        // {"34:b4:72:69:cb:9d", 0xc000, 1},
        // {"34:b4:72:69:e6:13", 0xc001, 1},
        // {"34:b4:72:6a:76:ed", 0xc002, 1},
        // {"34:b4:72:6a:77:c1", 0xc003, 1},
        // {"34:b4:72:6a:84:59", 0xc004, 1},
        {"f4:12:fa:ea:0b:65", 0x006e, 1},
        {"f4:12:fa:ea:0b:91", 0x008d, 1},
        {"f4:12:fa:ea:0b:75", 0x0090, 1},
        {"f4:12:fa:ea:0b:85", 0x006b, 1},
    };
    const size_t devices_len = sizeof(devices_confs) / sizeof(devices_confs[0]);
    for(size_t i = 0; i < devices_len; i++){
        const device_conf_t *d_conf = &devices_confs[i];
        devices.emplace_back(std::make_shared<Device>(i, DeviceType::Station, d_conf->mac, d_conf->wifi_channel, d_conf->ble_addr));
    }
    auto cli = Device::getSerialCli();
    if(cli){
        cli->startReadTask();
    }
    xTaskCreatePinnedToCore(task, "ImfHelper", 1024*32, NULL, tskIDLE_PRIORITY+1, NULL, 1);
}