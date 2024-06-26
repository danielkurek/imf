/**
 * @file board.c
 * @author Daniel Kurek (daniel.kurek.dev@gmail.com)
 * @brief Implementation of @ref board.h
 * @version 0.1
 * @date 2023-11-09
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#include "board.h"
#include "led_strip.h"
#include "driver/gpio.h"
#include "esp_log.h"

/**
 * @brief GPIO pin for controlling power of external board (configured via `idf.py menuconfig`)
 */
#define EXT_POWER_CTRL_GPIO CONFIG_EXT_POWER_GPIO

static const char* TAG = "BOARD";

rgb_t NONE  = {  0,  0,  0};
rgb_t RED   = {255,  0,  0};
rgb_t GREEN = {  0,255,  0};
rgb_t BLUE  = {  0,  0,255};
rgb_t WHITE = {255,255,255};

rgb_conf_t internal_rgb_conf = {
#if CONFIG_IDF_TARGET_ESP32C3
    .pin = GPIO_NUM_8,
#else
    .pin = GPIO_NUM_38,
#endif
    .color = {0,0,0},
    .number_of_leds = 1,
    .led_strip = 0,
    .on_off = 1,
};

static button_conf_t *s_buttons;
static size_t s_buttons_len;

static gpio_num_t s_ext_pwr_gpio;

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

esp_err_t board_set_onoff(rgb_conf_t *conf, bool onoff){
    conf->on_off = onoff;
    return update_led(conf);
}

esp_err_t board_set_ext_pwr(bool onoff){
    return gpio_set_level(s_ext_pwr_gpio, onoff);
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

void board_buttons_release_register_callback(board_button_callback_t callback){
    for(size_t i = 0; i < s_buttons_len; i++){
        s_buttons[i].callback = callback;
    }
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

static esp_err_t board_buttons_init(size_t buttons_len, const button_gpio_config_t *buttons){
    esp_err_t ret = ESP_OK;
    s_buttons = (button_conf_t*) malloc(buttons_len * sizeof(button_conf_t));
    if(!s_buttons){
        s_buttons_len = 0;
        ESP_LOGE(TAG, "Could not allocate buttons array");
        return ESP_FAIL;
    }
    s_buttons_len = buttons_len;

    for(size_t i = 0; i < buttons_len; i++){
        button_config_t button_config = {
            .type = BUTTON_TYPE_GPIO,
            .gpio_button_config = {
                .gpio_num = buttons[i].gpio_num,
                .active_level = buttons[i].active_level
            }
        };
        s_buttons[i].pin = buttons[i].gpio_num;
        s_buttons[i].button_num = i;
        s_buttons[i].handle = iot_button_create(&button_config);
        s_buttons[i].callback = NULL;
        if(s_buttons[i].handle == NULL){
            return ESP_FAIL;
        }

        esp_err_t err = iot_button_register_cb(s_buttons[i].handle, BUTTON_PRESS_UP, button_cb, &(s_buttons[i]));
        if(ret == ESP_OK && err != ESP_OK){
            ESP_LOGE(TAG, "Could not register iotButton callback");
            ret = err;
        }
    }
    return ret;
}

static esp_err_t board_ext_pwr_init(gpio_num_t gpio_num){
    gpio_config_t config = {
        .pin_bit_mask = 1ull << gpio_num,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    s_ext_pwr_gpio = gpio_num;
    return gpio_config(&config);
}

esp_err_t board_init(size_t buttons_len, const button_gpio_config_t *buttons){
    esp_err_t ret_err = ESP_OK;
    esp_err_t err;
    err = board_led_init(&internal_rgb_conf);
    if(err != ESP_OK){
        ESP_LOGE(TAG, "Could not init internal LED");
        ret_err = err;
    }

    err = board_buttons_init(buttons_len, buttons);
    if(err != ESP_OK){
        ESP_LOGE(TAG, "Could not init buttons");
        ret_err = err;
    }

    err = board_ext_pwr_init(EXT_POWER_CTRL_GPIO);
    if(err != ESP_OK){
        ESP_LOGE(TAG, "Could not init ext power control GPIO");
        ret_err = err;
    }

    return ret_err;
}