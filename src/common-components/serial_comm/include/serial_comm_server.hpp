#ifndef SERIAL_COMM_SERVER_H_
#define SERIAL_COMM_SERVER_H_

#include <inttypes.h>
#include <driver/uart.h>
#include <string>
#include "serial_comm_common.hpp"
#include <unordered_map>

namespace com{
    /**
     * @brief callback function when value of some field is changed
     */
    typedef void (*serial_comm_change_cb)(uint16_t addr, const std::string& field, const std::string& value);

    /**
     * @brief callback to retrieve new value for specified field
     */
    typedef void (*serial_comm_get_cb)(uint16_t addr, const std::string& field);

    class SerialCommSrv : public SerialComm {
        public:
            /**
             * @brief Construct a new Serial Comm Srv object
             * 
             * @param port UART port
             * @param tx_io_num GPIO pin of UART TX
             * @param rx_io_num GPIO pin of UART RX
             * @param default_addr address of this device
             */
            SerialCommSrv(const uart_port_t port, int tx_io_num, int rx_io_num, uint16_t default_addr);

            /**
             * @brief Get Field value of a device
             * 
             * @param[in] addr address of the device
             * @param[in] field field name of the device
             * @param[out] out resulting value
             * @return esp_err_t ESP_OK if value is found and returned
             */
            esp_err_t GetField(uint16_t addr, const std::string& field, std::string& out);

            /**
             * @brief Set Field value for a device
             * 
             * @param addr address of the device
             * @param field field name of the device
             * @param value new value of the field
             * @param do_callback perform serial_comm_change_cb callback?
             * @return esp_err_t ESP_OK if succeeds
             */
            esp_err_t SetField(uint16_t addr, const std::string& field, const std::string& value, bool do_callback = true);

            /**
             * @brief Register callbacks for changing fields values and retrieval of new value a field
             * 
             * @param change_cb change callback function or nullptr
             * @param get_cb get callback function or nullptr
             */
            void RegisterChangeCallback(serial_comm_change_cb change_cb, serial_comm_get_cb get_cb){
                _change_callback = change_cb;
                _get_callback = get_cb;
            }

            /**
             * @brief Process incoming messages (only @ref SerialRequest is allowed)
             * 
             * @param input incoming single message
             */
            void processInput(const std::string& input) override;
        private:

            /**
             * @brief Helper function to send current field value over UART
             * 
             * @param addr address of the device
             * @param field_name field name of the device
             * @return esp_err_t ESP_OK if message is sent
             */
            esp_err_t _sendField(uint16_t addr, const std::string& field_name);

            uint16_t _default_addr; /**< address of this device, it is added when no address is specified in SerialRequest */
            /**
             * @brief storage of field values
             */
            std::unordered_map<uint16_t, std::unordered_map<std::string, std::string>> fields;
            serial_comm_change_cb _change_callback = nullptr; /**< registered change callback */
            serial_comm_get_cb _get_callback = nullptr; /**< registered get callback */
            SemaphoreHandle_t _semMutex; /**< semaphore to synchronize writing and reading to/from storage */
    };
}

#endif