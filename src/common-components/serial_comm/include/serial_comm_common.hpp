#ifndef SERIAL_COMM_COMMON_H_
#define SERIAL_COMM_COMMON_H_

#include <string>

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

#endif