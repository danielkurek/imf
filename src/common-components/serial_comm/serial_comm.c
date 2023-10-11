#include "serial_comm.h"

typedef struct{
    uart_port_t port;
} serial_conf_t;

static serial_conf_t conf;

typedef char[8] serial_field_t;

typedef struct{
    cmd_type_t cmd;
    serial_field_t field;
    size_t payload_size;
    char[payload_size] payload;
} serial_msg_t;

bool serial_init(const uart_port_t port, int tx_io_num, int rx_io_num){
    conf.port = port;

    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    if(uart_param_config(port, &uart_config) != ESP_OK){
        ESP_LOGE(TAG, "cannot set UART params");
        return false;
    }

    if(uart_set_pin(port, tx_io_num, rx_io_num, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK){
        ESP_LOGE(TAG, "cannot set UART pins");
        return false;
    }

    const int uart_buffer_size = (1024 * 2);
    if(uart_driver_install(port, uart_buffer_size, uart_buffer_size, 10, &uart_queue, 0) != ESP_OK){
        ESP_LOGE(TAG, "cannot install UART driver");
        return false;
    }
    
    return true;
}

// implement async using events
// when message is received dispatch an event to user so that they can read it
// create multiple events
//  - MSG_RECEIVE
//  - MSG_SENT
//  - MSG_STATUS
int comm_send_cmd(cmd_type_t type, const serial_field_t field, const size_t body_len, char[body_len] body, char* output, const size_t output_len){
    serial_msg_t msg;

    msg.cmd = type;
    msg.field = field;
    msg.payload_size = body_len;
    msg.payload = body;

    uart_write_bytes(conf.port, (const char*) msg, sizeof(msg));
}

int comm_get_field(const serial_field_t field);

int comm_put_field(const serial_field_t field, const char[] value);

int comm_get_status();

int comm_send_status(int status);