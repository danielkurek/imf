#ifndef SERIAL_COMM_COMMON_H_
#define SERIAL_COMM_COMMON_H_

#include <iostream>

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

std::string GetCmdName(CmdType type) constexpr {
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

CmdType ParseCmdType(const std::string& cmdType) const {
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

std::string GetStatusName(CommStatus type) constexpr {
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

CommStatus ParseStatus(const std::string& status) const {
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

#endif