#include "serial_comm_common.hpp"
#include <cinttypes>

std::string GetCmdName(CmdType type) {
    switch (type)
    {
    case CmdType::GET:
        return "GET";
        break;
    case CmdType::PUT:
        return "PUT";
        break;
    case CmdType::STATUS:
        return "STATUS";
        break;
    default:
        return "UNKWN";
        break;
    }
}

CmdType ParseCmdType(const std::string& cmdType) {
    if(cmdType == "GET") {
        return CmdType::GET;
    }
    if(cmdType == "PUT") {
        return CmdType::PUT;
    }
    if(cmdType == "STATUS") {
        return CmdType::STATUS;
    }
    return CmdType::None;
}

std::string GetStatusName(CommStatus type) {
    switch (type)
    {
    case CommStatus::OK:
        return "OK";
        break;
    case CommStatus::FAIL:
        return "FAIL";
        break;
    case CommStatus::BOOTLOADER:
        return "BTLD";
        break;
    default:
        return "UNKWN";
        break;
    }
}

CommStatus ParseStatus(const std::string& status) {
    if(status == "OK"){
        return CommStatus::OK;
    }
    else if(status == "FAIL"){
        return CommStatus::OK;
    }
    else if(status == "BTLD"){
        return CommStatus::OK;
    }

    return CommStatus::None;
}

esp_err_t AddrToStr(uint16_t addr, std::string& out){
    char buf[5];
    int ret = snprintf(buf, 5, "%04" PRIx16, addr);
    if(ret > 0){
        out = std::string(buf);
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t StrToAddr(const std::string& addrStr, uint16_t *addrOut){
    int ret = sscanf(addrStr.c_str(), "%04" SCNx16, addrOut);

    if(ret == 1){
        return ESP_OK;
    }
    return ESP_FAIL;
}