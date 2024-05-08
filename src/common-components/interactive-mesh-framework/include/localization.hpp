/**
 * @file localization.hpp
 * @author Daniel Kurek (daniel.kurek.dev@gmail.com)
 * @brief Localization abstract class
 * @version 0.1
 * @date 2024-04-04
 * 
 * @copyright Copyright (c) 2024
 * 
 */
#ifndef LOCALIZATION_HPP_
#define LOCALIZATION_HPP_

#include "freertos/FreeRTOS.h"

namespace imf{
    class Localization{
        public:
            /**
             * @brief Start continuous localization
             * 
             * @return bool true if start was successful
             */
            virtual bool start() = 0;

            /**
             * @brief Stop continuous localization
             */
            virtual void stop() = 0;

            /**
             * @brief One loop of localization
             * 
             * @param diff time difference since last run
             */
            virtual void tick(TickType_t diff) = 0;
    };
}

#endif