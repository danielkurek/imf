idf_component_register(SRCS "logger.c"
                    INCLUDE_DIRS "include"
                    REQUIRES "esp_littlefs" driver vfs)