#ifndef SERIAL_COMM_CLIENT_H_
#define SERIAL_COMM_CLIENT_H_

#include <inttypes.h>
#include <driver/uart.h>
#include <string>
#include "serial_comm_common.hpp"
#include "freertos/semphr.h"
#include <unordered_map>

namespace com{
    typedef struct {
        TickType_t arrivalTime; /**< time of message arrival with current value */
        std::string value; /**< field value */
    } cache_value_t;

    class SerialCommCli : public SerialComm {
        public:
            /**
             * @brief Construct a new Serial Comm Cli object
             * 
             * @param port UART port
             * @param tx_io_num GPIO pin of UART TX
             * @param rx_io_num GPIO pin of UART RX
             * @param cache_threshold time threshold for value renewal in cache
             */
            SerialCommCli(const uart_port_t port, int tx_io_num, int rx_io_num, const TickType_t cache_threshold = 0);
            /**
             * @brief Get Field value
             * 
             * @param field field name
             * @return std::string value stored or empty string if nothing is found
             */
            std::string GetField(const std::string& field);

            /**
             * @brief Get Field value of a device
             * 
             * @param addr address of the device
             * @param field_name field name of the device
             * @return std::string value stored or empty string if nothing is found
             */
            std::string GetField(uint16_t addr, const std::string& field_name);

            /**
             * @brief Set Field value
             * 
             * @param field field name
             * @param value value of the field
             * @return esp_err_t ESP_OK if succeeds
             */
            esp_err_t PutField(const std::string& field, const std::string& value);

            /**
             * @brief Set Field value of a device
             * 
             * @param addr address of the device
             * @param field_name field name
             * @param value value of the field
             * @return esp_err_t ESP_OK if succeeds
             */
            esp_err_t PutField(uint16_t addr, const std::string& field_name, const std::string& value);
        private:
            /**
             * @brief Implement processing of incoming messages (only @ref com::SerialReponse)
             * 
             * @param input incoming single message
             */
            void processInput(const std::string& input) override;
            /**
             * @brief cache for storing values of parsed responses field_name->value
             */
            std::unordered_map<std::string, cache_value_t> cache{};
            SemaphoreHandle_t _semMutex; /**< semaphore to synchronize writing and reading to/from cache */
            TickType_t _cacheThreshold; /**< time threshold for value renewal in cache */
    };
}

#endif