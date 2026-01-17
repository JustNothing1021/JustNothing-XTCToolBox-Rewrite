/*****************************************************************************
 * sahara.cpp
 *
 * This class implements sahara protocol support
 *
 * Copyright (c) 2007-2011
 * Qualcomm Technologies Incorporated.
 * All Rights Reserved.
 * Qualcomm Confidential and Proprietary
 *
 *****************************************************************************/
 /*=============================================================================
                         Edit History

 $Header: //source/qcom/qct/platform/uefi/workspaces/pweber/apps/8x26_emmcdl/emmcdl/main/latest/src/sahara.cpp#9 $
 $DateTime: 2015/04/29 17:06:00 $ $Author: pweber $

 when       who     what, where, why
 -------------------------------------------------------------------------------
 30/10/12   pgw     Initial version.
 =============================================================================*/

#include "emmcdl_new/sahara.h"
#include "utils/logger.h"
#include "emmcdl_new/utils.h"
#include "utils/string_utils.h"

Sahara::Sahara(SerialPort* port, HANDLE hLogFile) {
    sport = port;
    hLog = hLogFile;
    
    if (!sport) {
        LERROR("Sahara::Sahara", "串口指针为空");
        throw std::invalid_argument("串口指针不能为空");
    }
}

void Sahara::Log(const char* str, ...) {
    va_list ap;
    va_start(ap, str);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), str, ap);
    LDEBUG("Sahara::Log", "%s", buffer);
    va_end(ap);
}

int Sahara::DeviceReset() {
    execute_cmd_t exe_cmd;
    exe_cmd.cmd = SAHARA_RESET_REQ;
    exe_cmd.len = 0x8;
    return sport->Write((BYTE*) &exe_cmd, sizeof(exe_cmd));
}

int Sahara::ReadData(int cmd, BYTE* buf, int len) {
    int status;
    DWORD bytesRead;

    execute_cmd_t exe_cmd;
    exe_cmd.cmd = SAHARA_EXECUTE_REQ;
    exe_cmd.len = sizeof(exe_cmd);
    exe_cmd.client_cmd = cmd;

    status = sport->Write((BYTE*) &exe_cmd, sizeof(exe_cmd));
    if (status != ERROR_SUCCESS) {
        LERROR("Sahara::ReadData", "发送执行请求失败: %s", 
               getErrorDescription(status).c_str());
        return -ERROR_INVALID_HANDLE;
    }

    execute_rsp_t exe_rsp;
    bytesRead = sizeof(exe_rsp);
    status = sport->Read((BYTE*) &exe_rsp, &bytesRead);
    if (exe_rsp.data_len > 0 && bytesRead == sizeof(exe_rsp)) {
        exe_cmd.cmd = SAHARA_EXECUTE_DATA;
        exe_cmd.len = sizeof(exe_cmd);
        exe_cmd.client_cmd = cmd;
        status = sport->Write((BYTE*) &exe_cmd, sizeof(exe_cmd));
        if (status != ERROR_SUCCESS) {
            LERROR("Sahara::ReadData", "发送执行数据请求失败: %s", 
                   getErrorDescription(status).c_str());
            return -ERROR_INVALID_HANDLE;
        }
        bytesRead = len;
        status = sport->Read((BYTE*) buf, &bytesRead);
        LDEBUG("Sahara::ReadData", "读取数据完成，实际读取字节数: %d", bytesRead);
        return bytesRead;
    }

    LWARN("Sahara::ReadData", "无效的响应数据，数据长度: %d, 读取字节数: %d", 
          exe_rsp.data_len, bytesRead);
    return -ERROR_INVALID_DATA;
}

int Sahara::DumpDeviceInfo(pbl_info_t* pbl_info) {
    DWORD dataBuf[64];
    int status = ERROR_SUCCESS;

    // 连接到设备的命令模式
    status = ConnectToDevice(true, 3);
    if (status != ERROR_SUCCESS) {
        LERROR("Sahara::DumpDeviceInfo", "连接到设备命令模式失败: %s", 
               getErrorDescription(status).c_str());
        return status;
    }

    // 确保收到命令就绪响应
    execute_rsp_t cmd_rdy;
    DWORD bytesRead = sizeof(cmd_rdy);

    status = sport->Read((BYTE*) &cmd_rdy, &bytesRead);
    if (status != ERROR_SUCCESS || bytesRead == 0) {
        LWARN("Sahara::DumpDeviceInfo", "切换模式命令后设备无响应");
        return ERROR_INVALID_PARAMETER;
    } else if (cmd_rdy.cmd != SAHARA_CMD_READY) {
        LWARN("Sahara::DumpDeviceInfo", "意外的响应命令码: %d (期望: SAHARA_CMD_READY)", cmd_rdy.cmd);
        return ERROR_INVALID_PARAMETER;
    }
    
    LDEBUG("Sahara::DumpDeviceInfo", "设备已就绪，开始读取设备信息");

    // 读取所有设备信息
    status = ReadData(SAHARA_EXEC_CMD_SERIAL_NUM_READ, (BYTE*) dataBuf, sizeof(dataBuf));
    if (status > 0 && status < sizeof(dataBuf)) {
        pbl_info->serial = dataBuf[0];
        LDEBUG("Sahara::DumpDeviceInfo", "序列号: 0x%08x", pbl_info->serial);
    } else {
        LWARN("Sahara::DumpDeviceInfo", "读取序列号失败，状态: %d", status);
    }
    
    status = ReadData(SAHARA_EXEC_CMD_MSM_HW_ID_READ, (BYTE*) dataBuf, sizeof(dataBuf));
    if (status > 0 && status < sizeof(dataBuf)) {
        pbl_info->msm_id = dataBuf[1];
        LDEBUG("Sahara::DumpDeviceInfo", "MSM硬件ID: 0x%08x", pbl_info->msm_id);
    } else {
        LWARN("Sahara::DumpDeviceInfo", "读取MSM硬件ID失败，状态: %d", status);
    }
    
    status = ReadData(SAHARA_EXEC_CMD_OEM_PK_HASH_READ, (BYTE*) dataBuf, sizeof(dataBuf));
    if (status > 0 && status < sizeof(dataBuf)) {
        memcpy_s(pbl_info->pk_hash, 32, dataBuf, sizeof(pbl_info->pk_hash));
        LDEBUG("Sahara::DumpDeviceInfo", "OEM公钥哈希已读取");
    } else {
        LWARN("Sahara::DumpDeviceInfo", "读取OEM公钥哈希失败，状态: %d", status);
    }
    
    status = ReadData(SAHARA_EXEC_CMD_GET_SOFTWARE_VERSION_SBL, (BYTE*) dataBuf, sizeof(dataBuf));
    if (status > 0 && status < sizeof(dataBuf)) {
        pbl_info->pbl_sw = dataBuf[0];
        LDEBUG("Sahara::DumpDeviceInfo", "SBL软件版本: 0x%08x", pbl_info->pbl_sw);
    } else {
        LWARN("Sahara::DumpDeviceInfo", "读取SBL软件版本失败，状态: %d", status);
    }

    ModeSwitch(0);
    LINFO("Sahara::DumpDeviceInfo", "设备信息读取完成");
    return ERROR_SUCCESS;
}

int Sahara::ModeSwitch(int mode) {
    // 最后发送切换模式命令，将设备恢复到原始EDL状态
    int status = ERROR_SUCCESS;
    execute_cmd_t cmd_switch_mode;
    BYTE read_data_req[256];
    DWORD bytesRead = sizeof(read_data_req);

    cmd_switch_mode.cmd = SAHARA_SWITCH_MODE;
    cmd_switch_mode.len = sizeof(cmd_switch_mode);
    cmd_switch_mode.client_cmd = mode;
    
    LDEBUG("Sahara::ModeSwitch", "发送模式切换命令，目标模式: %d", mode);
    status = sport->Write((BYTE*) &cmd_switch_mode, sizeof(cmd_switch_mode));
    if (status != ERROR_SUCCESS) {
        LERROR("Sahara::ModeSwitch", "发送模式切换命令失败: %s", 
               getErrorDescription(status).c_str());
        return status;
    }

    // 读取模式切换的响应（如果设备响应）
    status = sport->Read((BYTE*) &read_data_req, &bytesRead);
    if (status == ERROR_SUCCESS) {
        LDEBUG("Sahara::ModeSwitch", "模式切换成功，响应字节数: %d", bytesRead);
    } else {
        LWARN("Sahara::ModeSwitch", "模式切换响应读取失败: %s", 
              getErrorDescription(status).c_str());
    }
    
    return status;
}

int Sahara::LoadFlashProg(const std::string& szFlashPrg) {
    read_data_t read_data_req = {0};
    read_data_64_t read_data64_req = {0};
    cmd_hdr_t read_cmd_hdr = {0};
    image_end_t read_img_end = {0 };
    DWORD status = ERROR_SUCCESS;
    DWORD bytesRead = sizeof(read_data64_req);
    DWORD totalBytes = 0, read_data_offset = 0, read_data_len = 0;
    BYTE dataBuf[8192];

    HANDLE hFlashPrg = CreateFileA(szFlashPrg.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        0,
        NULL);
    if (hFlashPrg == INVALID_HANDLE_VALUE) {
        return ERROR_OPEN_FAILED;
    }

    LDEBUG("Sahara::LoadFlashProg", "成功打开烧录内核文件: \"%s\"", szFlashPrg.c_str());

    for (;;) {

        memset(&read_cmd_hdr, 0, sizeof(read_cmd_hdr));
        bytesRead = sizeof(read_cmd_hdr);
        status = sport->Read((BYTE*) &read_cmd_hdr, &bytesRead);

        // Check if it is a 32-bit or 64-bit read
        if (read_cmd_hdr.cmd == SAHARA_64BIT_MEMORY_READ_DATA) {
            memset(&read_data64_req, 0, sizeof(read_data64_req));
            bytesRead = sizeof(read_data64_req);
            status = sport->Read((BYTE*) &read_data64_req, &bytesRead);
            read_data_offset = (uint32_t) read_data64_req.data_offset;
            read_data_len = (uint32_t) read_data64_req.data_len;
        } else if (read_cmd_hdr.cmd == SAHARA_READ_DATA) {
            memset(&read_data_req, 0, sizeof(read_data_req));
            bytesRead = sizeof(read_data_req);
            status = sport->Read((BYTE*) &read_data_req, &bytesRead);
            read_data_offset = read_data_req.data_offset;
            read_data_len = read_data_req.data_len;
        } else {
            // Didn't find any read data command so break
            break;
        }

        if (SetFilePointer(hFlashPrg, read_data_offset, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER) {
            LWARN("Sahara::LoadFlashProg", "更新烧录内核文件指针偏移失败");
            CloseHandle(hFlashPrg);
            return GetLastError();
        }

        if (!ReadFile(hFlashPrg, dataBuf, read_data_len, &bytesRead, NULL)) {
            LWARN("Sahara::LoadFlashProg", "从烧录内核文件读取数据失败");
            CloseHandle(hFlashPrg);
            return GetLastError();
        }

        if (sport->Write(dataBuf, bytesRead) != ERROR_SUCCESS) {
            LWARN("Sahara::LoadFlashProg", "向设备写入数据时失败");
            CloseHandle(hFlashPrg);
            return GetLastError();
        }
        totalBytes += bytesRead;
    }

    CloseHandle(hFlashPrg);

    if (read_cmd_hdr.cmd != SAHARA_END_TRANSFER) {
        LWARN("Sahara::LoadFlashProg", "预期收到SAHARA_END_TRANSFER命令 (%d)，但实际收到了%d", SAHARA_END_TRANSFER, read_cmd_hdr.cmd);
        return ERROR_WRITE_FAULT;
    }

    bytesRead = sizeof(read_img_end);
    status = sport->Read((BYTE*) &read_img_end, &bytesRead);
    if (read_img_end.status != SAHARA_ERROR_SUCCESS) {
        LWARN("Sahara::LoadFlashProg", "烧录内核失败，错误码: %d", read_img_end.status);
        return read_img_end.status;
    }

    done_t done_pkt = { 0 };
    done_pkt.cmd = SAHARA_DONE_REQ;
    done_pkt.len = 8;
    status = sport->Write((BYTE*) &done_pkt, 8);
    if (status != ERROR_SUCCESS) {
        return status;
    }

    bytesRead = sizeof(done_pkt);
    status = sport->Read((BYTE*) &done_pkt, &bytesRead);
    if (done_pkt.cmd != SAHARA_DONE_RSP) {
        LWARN("Sahara::LoadFlashProg", "预期收到SAHARA_DONE_RSP命令，但实际收到到了 %d", done_pkt.cmd);
        return ERROR_WRITE_FAULT;
    }

    sport->SetTimeout(500);
    Sleep(500);
    return status;
}

bool Sahara::CheckDevice(void) {
    hello_req_t hello_req = { 0 };
    int status = ERROR_SUCCESS;
    DWORD bytesRead = sizeof(hello_req);
    sport->SetTimeout(1);
    status = sport->Read((BYTE*) &hello_req, &bytesRead);
    sport->SetTimeout(1000);
    return (hello_req.cmd == SAHARA_HELLO_REQ);
}

int Sahara::ConnectToDevice(bool bReadHello, int mode) {
    hello_req_t hello_req = { 0 };
    int status = ERROR_SUCCESS;
    DWORD bytesRead = sizeof(hello_req);

    if (bReadHello) {
        sport->SetTimeout(10);
        status = sport->Read((BYTE*) &hello_req, &bytesRead);
        if (hello_req.cmd != SAHARA_HELLO_REQ) {
            LWARN("Sahara::ConnectToDevice", "未收到设备的Sahara协议Hello数据包, 状态: %s", 
                getErrorDescription(status).c_str());
            LWARN("Sahara::ConnectToDevice", "接收到的数据: \n%s", bytesRead ? string_utils::to_hex_view((char*) &hello_req, bytesRead).c_str() : "<空>");
            return ERROR_INVALID_HANDLE;
        }
        sport->SetTimeout(2000);
    } else {
        sport->Flush();
    }

    hello_req.cmd = SAHARA_HELLO_RSP;
    hello_req.len = 0x30;
    hello_req.version = 2;
    hello_req.version_min = 1;
    hello_req.max_cmd_len = 0;
    hello_req.mode = mode;

    status = sport->Write((BYTE*) &hello_req, sizeof(hello_req));

    if (status != ERROR_SUCCESS) {
        LWARN("Sahara::ConnectToDevice", "向设备发送Hello响应失败, 状态: %s", 
            getErrorDescription(status).c_str());
        return ERROR_INVALID_HANDLE;
    }

    return ERROR_SUCCESS;
}

// This function is to fix issue where PBL does not propery handle PIPE reset need to make sure 1 TX and 1 RX is working we may be out of sync...
// 这个函数是为了防止管道卡住的问题出现
int Sahara::PblHack(void) {
    int status = ERROR_SUCCESS;

    // Assume that we already got the hello req so send hello response
    status = ConnectToDevice(false, 3);
    if (status != ERROR_SUCCESS) {
        return status;
    }

    // Make sure we get command ready back
    execute_rsp_t cmd_rdy;
    DWORD bytesRead = sizeof(cmd_rdy);

    sport->SetTimeout(10);
    status = sport->Read((BYTE*) &cmd_rdy, &bytesRead);
    if (status != ERROR_SUCCESS || bytesRead == 0) {
        return ModeSwitch(0);
    } else if (cmd_rdy.cmd != SAHARA_CMD_READY) {
        LWARN("Sahara::PblHack", "PblHack出错，没有出现错误却读取到了数据，cmd_rdy.cmd = %d", cmd_rdy.cmd);
        LWARN("Sahara::PblHack", "读取到的数据: \n%s", string_utils::to_hex_view((char*) &cmd_rdy, sizeof(cmd_rdy)).c_str());
        return ERROR_INVALID_DATA;
    }

    return ModeSwitch(0);
}