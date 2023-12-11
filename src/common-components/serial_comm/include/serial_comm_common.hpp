#ifndef SERIAL_COMM_COMMON_H_
#define SERIAL_COMM_COMMON_H_

#include <string>
#include "esp_err.h"

enum class CmdType{
    None = 0,
    GET,
    PUT,
    STATUS,
};

enum class CommStatus{
    None = 0,
    OK,
    FAIL,
    BOOTLOADER,
};

std::string GetCmdName(CmdType type);

CmdType ParseCmdType(const std::string& cmdType);

std::string GetStatusName(CommStatus type);

CommStatus ParseStatus(const std::string& status);

esp_err_t AddrToStr(uint16_t addr, std::string& out);

esp_err_t StrToAddr(std::string addrStr, uint16_t *addrOut);

#endif