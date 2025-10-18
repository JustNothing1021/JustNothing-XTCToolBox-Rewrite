
#include "utils/usb_utils.h"
#include "utils/string_utils.h"

using namespace std;

// DeepSeek写的（
vector<usb_utils::COMPortInfo> usb_utils::GetCOMPorts() {
    vector<COMPortInfo> comPorts;

    HDEVINFO hDevInfo = SetupDiGetClassDevs(
        &GUID_DEVINTERFACE_COMPORT,
        NULL,
        NULL,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE
    );

    if (hDevInfo == INVALID_HANDLE_VALUE) {
        return comPorts;
    }

    SP_DEVICE_INTERFACE_DATA deviceInterfaceData;
    deviceInterfaceData.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    // 枚举所有设备
    for (DWORD i = 0; SetupDiEnumDeviceInterfaces(hDevInfo, NULL,
        &GUID_DEVINTERFACE_COMPORT, i, &deviceInterfaceData); i++) {
        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetailA(hDevInfo, &deviceInterfaceData,
            NULL, 0, &requiredSize, NULL);
        if (requiredSize == 0) continue;

        vector<BYTE> detailBuffer(requiredSize);
        PSP_DEVICE_INTERFACE_DETAIL_DATA_A pDetail =
            reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_A>(detailBuffer.data());
        pDetail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);

        SP_DEVINFO_DATA devInfoData;
        devInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

        if (SetupDiGetDeviceInterfaceDetailA(hDevInfo, &deviceInterfaceData,
            pDetail, requiredSize, NULL, &devInfoData)) {

            WCHAR friendlyName[256];
            DWORD friendlyNameSize = sizeof(friendlyName);
            if (SetupDiGetDeviceRegistryPropertyW(hDevInfo, &devInfoData,
                SPDRP_FRIENDLYNAME, NULL,
                (PBYTE) friendlyName, friendlyNameSize, NULL)) {

                string friendlyNameStr = string_utils::wstr2str(friendlyName);

                // 从友好名称中提取COM端口号
                size_t comPos = friendlyNameStr.find("(COM");
                if (comPos != string::npos) {
                    size_t endPos = friendlyNameStr.find(")", comPos);
                    if (endPos != string::npos) {
                        string comStr = friendlyNameStr.substr(comPos + 4,
                            endPos - comPos - 4);
                        try {
                            int portNum = stoi(comStr);
                            if (portNum >= 1 && portNum <= 19) {
                                // 提取设备名称（去掉COM端口部分）
                                string deviceName = friendlyNameStr.substr(0, comPos);

                                // 去除尾部空格
                                size_t lastChar = deviceName.find_last_not_of(" ");
                                if (lastChar != string::npos) {
                                    deviceName = deviceName.substr(0, lastChar + 1);
                                }

                                comPorts.push_back({ portNum, deviceName });
                            }
                        } catch (const exception&) {
                            // 忽略无法转换为数字的端口号然后输出警告信息
                            // LWARN(...);
                        }
                    }
                }
            }
        }
    }
    SetupDiDestroyDeviceInfoList(hDevInfo);
    return comPorts;
}

