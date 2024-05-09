
#include "interactive-mesh-framework.hpp"
#include "driver/gpio.h"
#include "esp_event.h"

// used for logging using ESP_LOG*, this does not save to storage
#include "esp_log.h"

// Tag for logging messages
static const char *TAG = "APP";

using namespace imf;

// pointer to access framework globally 
static std::unique_ptr<IMF> s_imf;

// Add buttons that will be used in this framework
static const std::vector<button_gpio_config_t> buttons = {
    {.gpio_num = GPIO_NUM_0 , .active_level = 0}, // adds boot button that is on every device
};

// Event handler for distance measurements
extern "C" void event_handler(void* event_handler_arg, esp_event_base_t event_base,
                   int32_t event_id, void* event_data){
    // Check that the event is from distance measurement
    if(event_base == DM_EVENT){
        dm_measurement_data_t *dm_measurement;
        dm_nearest_device_change_t *dm_nearest_dev;
        // Check which event was generated
        switch(event_id){
            case DM_MEASUREMENT_DONE:
                dm_measurement = (dm_measurement_data_t*) event_data;
                if(dm_measurement->valid){
                    LOGGER_I(TAG, "DM_MEASUREMENT_DONE, id=%" PRIu32 ", distance_cm=%" PRIu32 " rssi=%" PRId8, dm_measurement->point_id, dm_measurement->measurement.distance_cm, dm_measurement->measurement.rssi);
                } else{
                    LOGGER_I(TAG, "DM_MEASUREMENT_DONE, id=%" PRIu32 ", INVALID", dm_measurement->point_id);
                }
                break;
            case DM_NEAREST_DEVICE_CHANGE:
                dm_nearest_dev = (dm_nearest_device_change_t*) event_data;
                LOGGER_I(TAG, "DM_NEAREST_DEVICE_CHANGE, from: %" PRIx32 " | to: %" PRIx32, dm_nearest_dev->old_point_id, dm_nearest_dev->new_point_id);
                break;
            default:
                LOGGER_I(TAG, "Unknown event of DM, id=%" PRId32, event_id);
        }
    }
}

extern "C" void button_cb(uint8_t button_num){
    ESP_LOGI(TAG, "Button no. %d was pressed", button_num);
    if(button_num == 0){
        // First button was pressed (in our case Boot button)
        // for example change state of this device
    }
}

// update callback from IMF
void update_cb(TickType_t diff_ms){
    LOGGER_I(TAG, "Time since last update: %" PRIu32, diff_ms);
    // Do something (do not block for too long since it would delay internal functions of IMF)
}

// state change callback from IMF
void state_change_cb(uint32_t old_state, uint32_t new_state){
    LOGGER_I(TAG, "State was changed from %" PRIu32 " to %" PRIu32, old_state, new_state);
    // Do something
}

// start configuration mode if conditions are met
// This example uses pressing boot button shortly after startup to enter configuration mode
bool web_config(){

    // Configure GPIO pin that is connected to Boot button
    gpio_config_t config;

    config.intr_type    = GPIO_INTR_DISABLE;
    config.mode         = GPIO_MODE_INPUT;
    config.pin_bit_mask = (1ull << GPIO_NUM_0);
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.pull_up_en   = GPIO_PULLUP_ENABLE;

    gpio_config(&config);

    // Wait 1s
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    // Check if button is pressed down
    if(gpio_get_level(GPIO_NUM_0) == 0){
        LOGGER_I(TAG, "Booting to web config");
        s_imf->startWebConfig();
        return true; // do not start IMF, configuration mode
    }
    return false; // continue normally
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Startup!");
    
    // Create instance of IMF
    s_imf = std::make_unique<IMF>(buttons);
    if(!s_imf){
        ESP_LOGE(TAG, "Could not init IMF!");
        return;
    }

    // check if conditions are met for configuration mode
    if(web_config()){
        return;
    }

    ESP_LOGI(TAG, "init IMF");

    ESP_LOGI(TAG, "IMF add devices");
    // Add your devices' configuration to IMF
    // e.g. (change to your configuration)
    s_imf->addDevice(DeviceType::Station, "F4:12:FA:EA:0B:91", 1, 0x0002);
    s_imf->addDevice(DeviceType::Mobile , "F4:12:FA:EA:0B:85", 1, 0x0005);

    ESP_LOGI(TAG, "IMF register callbacks");
    // Register callbacks
    // this example registers all callbacks, if you do not want some, pass nullptr instead of the function name
    s_imf->registerCallbacks(button_cb, event_handler, NULL, update_cb, state_change_cb);

    ESP_LOGI(TAG, "IMF start");
    // Start framework
    s_imf->start();
}