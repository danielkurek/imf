idf_component_register(SRCS "board.c"
                    INCLUDE_DIRS "include"
                    REQUIRES button led_strip color driver esp_timer)