#ifndef SERIAL_COMM_COMMON_H_
#define SERIAL_COMM_COMMON_H_

#include <string>
#include "esp_err.h"
#include <functional>
#include <driver/uart.h>
#include "freertos/semphr.h"
#include "freertos/FreeRTOS.h"

namespace com{
    /**
     * @brief length of string representation of Bluetooth mesh address
     */
    static constexpr size_t addr_str_len = 4+1;

    enum class CmdType{
        None = 0,
        GET,
        PUT,
        STATUS,
    };

    enum class FieldParseErr{
        ok = 0,
        no_addr,
        malformed_addr,
        empty_field,
        empty_field_name,
    };

    /**
     * @brief CmdType to string
     * 
     * @param type type to convert to string
     * @return std::string 
     */
    std::string GetCmdName(CmdType type);

    /**
     * @brief Parse string to CmdType
     * 
     * @param cmdType string representation of CmdType
     * @return CmdType resulting type
     */
    CmdType ParseCmdType(const std::string& cmdType);

    /**
     * @brief Convert Bluetooth mesh address to correct format
     * 
     * @param[in] addr Bluetooth mesh address
     * @param[out] out string representation of the address
     * @return esp_err_t ESP_OK if succeeds
     */
    esp_err_t AddrToStr(uint16_t addr, std::string& out);

    /**
     * @brief Parse Bluetooth mesh address from string
     * 
     * @param[in] addrStr input string to parse
     * @param[out] addrOut resulting address
     * @return esp_err_t ESP_OK if succeeds
     */
    esp_err_t StrToAddr(const char *addrStr, uint16_t *addrOut);

    /**
     * @brief Create field name from its parts
     * 
     * @param[in] addr Bluetooth mesh address
     * @param[in] field name of a field
     * @param[out] out resulting string representation of the field
     * @return esp_err_t ESP_OK if succeeds
     */
    esp_err_t MakeField(uint16_t addr, const std::string& field, std::string& out);

    /**
     * @brief Parse field name to its parts
     * 
     * @param[in] input string representation of the field
     * @param[out] field field name
     * @param[out] addr address
     * @return FieldParseErr returns FieldParseErr::ok if succeeds
     */
    FieldParseErr ParseField(const std::string& input, std::string& field, uint16_t& addr);

    struct SerialRequest{
        CmdType type; /**< Request type */
        std::string field; /**< field name */
        std::string value; /**< only if @p type is CmdType::PUT */
        /**
         * @brief Serialize SerialRequest to command that can be sent
         * 
         * @return std::string SerialRequest string representation
         */
        std::string toString() const;
        /**
         * @brief Parse toString() object representation
         * 
         * @param[in] input string representation of SerialRequest
         * @param[out] out parsed request
         * @return esp_err_t ESP_OK if succeeds
         */
        static esp_err_t parse(const std::string& input, SerialRequest& out);
    };
    struct SerialResponse{
        std::string field; /**< field name */
        std::string value; /**< field value */

        /**
         * @brief Serialize SerialResponse to command that can be sent
         * 
         * @return std::string 
         */
        std::string toString() const;

        /**
         * @brief Parse toString() object representation
         * 
         * @param[in] input string representation
         * @param[out] out parsed result
         * @return esp_err_t ESP_OK if succeeds
         */
        static esp_err_t parse(const std::string& input, SerialResponse& out);
    };

    /**
     * @brief Base class for serial communication
     */
    class SerialComm{
        public:
            /**
             * @brief Construct a new Serial Comm object
             * 
             * @param port UART port
             * @param tx_io_num GPIO pin of UART TX
             * @param rx_io_num GPIO pin of UART RX
             * @param sep char that separates individual messages
             */
            SerialComm(const uart_port_t port, int tx_io_num, int rx_io_num, char sep='\n');

            /**
             * @brief Start thread for reading responses
             * 
             * @return esp_err_t ESP_OK if succeeds
             */
            esp_err_t startReadTask();

            /**
             * @brief Destroy thread for reading responses
             */
            void stopReadTask();

            /**
             * @brief Send request over UART
             * 
             * @param req request data
             * @return esp_err_t ESP_OK if succeeds
             */
            esp_err_t writeRequest(const SerialRequest& req);

            /**
             * @brief Send response over UART
             * 
             * @param res response data
             * @return esp_err_t ESP_OK if succeeds
             */
            esp_err_t writeResponse(const SerialResponse& res);

            /**
             * @brief Process received responses
             */
            virtual void processInput(const std::string&) = 0;
        private:
            /**
             * @brief Helper function to write to UART
             * 
             * @param data data to write to UART
             * @return esp_err_t ESP_OK if succeeds
             */
            esp_err_t write(const std::string& data);

            /**
             * @brief Necessary wrapper for creating thread that performs object's method
             * @param param pointer to SerialComm (usually @p this )
             */
            static void readTaskWrapper(void* param){
                static_cast<SerialComm *>(param)->readTask();
            }

            /**
             * @brief Read responses from UART
             */
            void readTask();

            /**
             * @brief Split reading buffer to individual messages
             * 
             * @param input reading buffer
             * @return int position the end of last message processed from buffer
             */
            int processInputs(const std::string& input);
            
            SemaphoreHandle_t _semMutex; /**< semaphore for synchronizing writes to UART */
            TaskHandle_t _xHandle = NULL; /**< handle for thread created in startReadTask() */
            uart_port_t _uart_port; /**< UART port used for communication */
            QueueHandle_t _uart_queue; /** queue for UART events */
            char _sep; /**< separation char between messages */
    };
}

#endif