#include "board.h"
#include "led_strip.h"
#include <driver/gpio.h>

rgb_conf_t internal_rgb_conf = {
    .pin = GPIO_NUM_38,
    .color = {0,0,0},
    .number_of_leds = 1,
    .led_strip = 0
};

static button_conf_t buttons[] = {
    {
        .pin = GPIO_NUM_46,
        .button_num = 0,
    },
    {
        .pin = GPIO_NUM_47,
        .button_num = 1,
    },
    {
        .pin = GPIO_NUM_48,
        .button_num = 2,
    },
    {
        .pin = GPIO_NUM_45,
        .button_num = 3,
    }
};

static size_t buttons_len = sizeof(buttons) / sizeof(buttons[0]);

esp_err_t board_set_rgb(rgb_conf_t *conf, rgb_t new_color){
    for(uint32_t i = 0; i < conf->number_of_leds; i++){
        led_strip_set_pixel(conf->led_strip, i, new_color.red, new_color.green, new_color.blue);
    }
    led_strip_refresh(conf->led_strip);
    return ESP_OK;
}

esp_err_t board_register_buttons_release_callback(board_button_callback_t callback){
    for(size_t i = 0; i < buttons_len; i++){
        buttons[i].callback = callback;
    }
    return ESP_OK;
}

static esp_err_t board_led_init(){
    led_strip_config_t strip_config = {
        .strip_gpio_num = internal_rgb_conf.pin, // The GPIO that connected to the LED strip's data line
        .max_leds = 1, // The number of LEDs in the strip,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB, // Pixel format of your LED strip
        .led_model = LED_MODEL_SK6812, // LED strip model
        .flags.invert_out = false, // whether to invert the output signal (useful when your hardware has a level inverter)
    };
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT, // different clock source can lead to different power consumption
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = false, // whether to enable the DMA feature
    };
    return led_strip_new_rmt_device(&strip_config, &rmt_config, &(internal_rgb_conf.led_strip));
}

static void button_cb(void* button_handle, void* user_data){
    if(user_data == NULL){
        return;
    }
    
    button_conf_t *button = (button_conf_t*) user_data;

    if(button->callback == NULL){
        return;
    }
    (button->callback)(button->button_num);
}

static esp_err_t board_buttons_init(){
    esp_err_t ret = ESP_OK;
    for(size_t i = 0; i < buttons_len; i++){
        button_config_t button_config = {
            .type = BUTTON_TYPE_GPIO,
            .gpio_button_config = {
                .gpio_num = buttons[i].pin,
                .active_level = 1
            }
        };

        buttons[i].handle = iot_button_create(&button_config);
        if(buttons[i].handle == NULL){
            return ESP_FAIL;
        }

        esp_err_t err = iot_button_register_cb(buttons[i].handle, BUTTON_PRESS_UP, button_cb, &buttons[i]);
        if(ret == ESP_OK && err != ESP_OK){
            ret = err;
        }
    }
    return ret;
}

esp_err_t board_init(){
    esp_err_t err;
    err = board_led_init();

    err = board_buttons_init();
    return err;
}