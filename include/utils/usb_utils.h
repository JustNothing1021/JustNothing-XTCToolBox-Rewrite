
#pragma once
#ifndef USB_UTILS_H
#define USB_UTILS_H



#include <windows.h>
#include <setupapi.h>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

#pragma comment(lib, "setupapi.lib")

#ifdef __MINGW32__
// MinGW下没有定义这些常量，得手动定义
const GUID GUID_DEVINTERFACE_COMPORT =
    { 0x86e0d1e0, 0x8089, 0x11d0, { 0x9c, 0xe4, 0x08, 0x00, 0x3e, 0x30, 0x1f, 0x73 } };
#endif


namespace usb_utils {

    // 端口信息结构体。
    struct COMPortInfo {
        int portNumber;           // 端口号，例如1表示COM1
        std::string deviceName;   // 设备名称，例如"USB-SERIAL CH340" 或者 "Qualcomm HS-USB Diagnostics 9008"
    };

    // 获取系统中所有可用的COM端口信息。
    std::vector<COMPortInfo> GetCOMPorts();

}
#endif // USB_UTILS_H
