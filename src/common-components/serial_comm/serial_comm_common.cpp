#include "serial_comm_common.hpp"

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