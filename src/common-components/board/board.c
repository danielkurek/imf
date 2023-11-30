#include "board.h"
#include "led_strip.h"
#include <driver/gpio.h>
#include "esp_log.h"

static const char* TAG = "BOARD";

rgb_t NONE  = {  0,  0,  0};
rgb_t RED   = {255,  0,  0};
rgb_t GREEN = {  0,255,  0};
rgb_t BLUE  = {  0,  0,255};
rgb_t WHITE = {255,255,255};

rgb_conf_t internal_rgb_conf = {
    .pin = GPIO_NUM_38,
    .color = {0,0,0},
    .number_of_leds = 1,
    .led_strip = 0,
    .on_off = 1,
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

esp_err_t update_led(rgb_conf_t *conf){
    if(!conf->on_off){
        return led_strip_clear(conf->led_strip);
    }
    esp_err_t err = ESP_OK;
    esp_err_t tmp_err;

    for(uint32_t i = 0; i < conf->number_of_leds; i++){
        tmp_err = led_strip_set_pixel(conf->led_strip, i, conf->color.red, conf->color.green, conf->color.blue);
        if(tmp_err != ESP_OK && err == ESP_OK){
            err = tmp_err;
        }
    }
    tmp_err = led_strip_refresh(conf->led_strip);
    if(tmp_err != ESP_OK && err == ESP_OK){
        err = tmp_err;
    }
    return err;
}

esp_err_t board_set_rgb(rgb_conf_t *conf, rgb_t new_color){
    conf->color = new_color;
    return update_led(conf);
}

esp_err_t board_start_blinking(rgb_conf_t *conf, uint64_t period_us){
    return esp_timer_start_periodic(conf->timer, period_us);
}

esp_err_t board_stop_blinking(rgb_conf_t *conf){
    return esp_timer_stop(conf->timer);
}

static void blink_cb(void *arg){
    rgb_conf_t *conf = (rgb_conf_t*) arg;
    conf->on_off = !conf->on_off;
    update_led(conf);
}

esp_err_t board_led_init(rgb_conf_t *conf){
    esp_err_t err;

    led_strip_config_t strip_config = {
        .strip_gpio_num = conf->pin, // The GPIO that connected to the LED strip's data line
        .max_leds = conf->number_of_leds, // The number of LEDs in the strip,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB, // Pixel format of your LED strip
        .led_model = LED_MODEL_SK6812, // LED strip model
        .flags.invert_out = false, // whether to invert the output signal (useful when your hardware has a level inverter)
    };
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT, // different clock source can lead to different power consumption
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = false, // whether to enable the DMA feature
    };
    
    err = led_strip_new_rmt_device(&strip_config, &rmt_config, &(conf->led_strip));
    if(err != ESP_OK){
        ESP_LOGE(TAG, "Failed initializing led driver! %s", esp_err_to_name(err));
        return err;
    }

    esp_timer_create_args_t args = {
        .callback = blink_cb,
        .name = "BlinkTimer",
        .arg = conf,
    };
    
    err = esp_timer_create(&args, &(conf->timer));
    if(err != ESP_OK){
        ESP_LOGE(TAG, "Failed initializing led timer! %s", esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}

esp_err_t board_buttons_release_register_callback(board_button_callback_t callback){
    for(size_t i = 0; i < buttons_len; i++){
        buttons[i].callback = callback;
    }
    return ESP_OK;
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
    err = board_led_init(&internal_rgb_conf);

    err = board_buttons_init();
    return err;
}