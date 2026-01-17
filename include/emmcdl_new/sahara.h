/*****************************************************************************
 * sahara.h
 *
 * This file implements the streaming download protocol for Qualcomm devices
 * 本文件实现了高通设备的流式下载协议（Sahara 协议）
 *
 * Copyright (c) 2007-2012
 * Qualcomm Technologies Incorporated.
 * All Rights Reserved.
 * Qualcomm Confidential and Proprietary
 *
 *****************************************************************************/
 /*=============================================================================
                         Edit History

 $Header: //source/qcom/qct/platform/uefi/workspaces/pweber/apps/8x26_emmcdl/emmcdl/main/latest/inc/sahara.h#5 $
 $DateTime: 2015/04/29 17:06:00 $ $Author: pweber $

 when       who     what, where, why
 -------------------------------------------------------------------------------
 10/30/12   pgw     Initial version.
 =============================================================================*/
#pragma once

#include "serialport.h"
#include "datatypes/bytearray.h"
#include <Windows.h>
#include <stdint.h>

// Sahara protocol command codes
// Sahara 协议命令码
#define SAHARA_HELLO_REQ      0x1   // Hello request / 握手请求
#define SAHARA_HELLO_RSP      0x2   // Hello response / 握手响应
#define SAHARA_READ_DATA      0x3   // Read data request / 读取数据请求
#define SAHARA_END_TRANSFER   0x4   // End transfer / 结束传输
#define SAHARA_DONE_REQ       0x5   // Done request / 完成请求
#define SAHARA_DONE_RSP       0x6   // Done response / 完成响应
#define SAHARA_RESET_REQ      0x7   // Reset request / 重置请求
#define SAHARA_RESET_RSP      0x8   // Reset response / 重置响应
#define SAHARA_MEMORY_DEBUG   0x9   // Memory debug / 内存调试
#define SAHARA_MEMORY_READ    0xA   // Memory read / 内存读取
#define SAHARA_CMD_READY      0xB   // Command ready / 命令就绪
#define SAHARA_SWITCH_MODE    0xC   // Switch mode / 切换模式
#define SAHARA_EXECUTE_REQ    0xD   // Execute request / 执行请求
#define SAHARA_EXECUTE_RSP    0xE   // Execute response / 执行响应
#define SAHARA_EXECUTE_DATA   0xF   // Execute data / 执行数据
#define SAHARA_64BIT_MEMORY_DEBUG		0x10  // 64-bit memory debug / 64位内存调试
#define SAHARA_64BIT_MEMORY_READ		0x11  // 64-bit memory read / 64位内存读取
#define SAHARA_64BIT_MEMORY_READ_DATA	0x12  // 64-bit memory read data / 64位内存读取数据

#define SAHARA_ERROR_SUCCESS  0x0   // Success error code / 成功错误码

// Sahara execute command IDs
// Sahara 执行命令 ID
enum boot_sahara_exec_cmd_id {
    SAHARA_EXEC_CMD_NOP = 0x00,                           // No operation / 空操作
    SAHARA_EXEC_CMD_SERIAL_NUM_READ = 0x01,               // Read serial number / 读取序列号
    SAHARA_EXEC_CMD_MSM_HW_ID_READ = 0x02,                // Read MSM hardware ID / 读取 MSM 硬件 ID
    SAHARA_EXEC_CMD_OEM_PK_HASH_READ = 0x03,              // Read OEM public key hash / 读取 OEM 公钥哈希
    SAHARA_EXEC_CMD_SWITCH_TO_DMSS_DLOAD = 0x04,         // Switch to DMSS DLOAD mode / 切换到 DMSS DLOAD 模式
    SAHARA_EXEC_CMD_SWITCH_TO_STREAM_DLOAD = 0x05,        // Switch to stream DLOAD mode / 切换到流式 DLOAD 模式
    SAHARA_EXEC_CMD_READ_DEBUG_DATA = 0x06,               // Read debug data / 读取调试数据
    SAHARA_EXEC_CMD_GET_SOFTWARE_VERSION_SBL = 0x07,      // Get software version of SBL / 获取 SBL 软件版本

    /* place all new commands above this */
    SAHARA_EXEC_CMD_LAST,
    SAHARA_EXEC_CMD_MAX = 0x7FFFFFFF
};

/**
 * @struct hello_req_t
 * @brief Sahara hello request structure.
 *        Sahara 握手请求结构体。
 */
typedef struct {
    DWORD cmd;           // Command code / 命令码
    DWORD len;           // Length of the packet / 数据包长度
    DWORD version;       // Protocol version / 协议版本
    DWORD version_min;   // Minimum supported version / 支持的最小版本
    DWORD max_cmd_len;   // Maximum command length / 最大命令长度
    DWORD mode;          // Mode / 模式
    DWORD res1;          // Reserved / 保留字段
    DWORD res2;          // Reserved / 保留字段
    DWORD res3;          // Reserved / 保留字段
    DWORD res4;          // Reserved / 保留字段
    DWORD res5;          // Reserved / 保留字段
    DWORD res6;          // Reserved / 保留字段
} hello_req_t;

/**
 * @struct cmd_hdr_t
 * @brief Sahara command header structure.
 *        Sahara 命令头结构体。
 */
typedef struct {
    DWORD cmd;           // Command code / 命令码
    DWORD len;           // Length of the packet / 数据包长度
} cmd_hdr_t;

/**
 * @struct read_data_t
 * @brief Sahara read data request structure (32-bit).
 *        Sahara 读取数据请求结构体（32位）。
 */
typedef struct {
    DWORD id;            // Image ID / 镜像 ID
    DWORD data_offset;   // Data offset / 数据偏移量
    DWORD data_len;      // Data length / 数据长度
} read_data_t;

/**
 * @struct read_data_64_t
 * @brief Sahara read data request structure (64-bit).
 *        Sahara 读取数据请求结构体（64位）。
 */
typedef struct {
    uint64_t id;         // Image ID / 镜像 ID
    uint64_t data_offset; // Data offset / 数据偏移量
    uint64_t data_len;   // Data length / 数据长度
} read_data_64_t;

/**
 * @struct execute_cmd_t
 * @brief Sahara execute command request structure.
 *        Sahara 执行命令请求结构体。
 */
typedef struct {
    DWORD cmd;           // Command code / 命令码
    DWORD len;           // Length of the packet / 数据包长度
    DWORD client_cmd;    // Client command / 客户端命令
} execute_cmd_t;

/**
 * @struct execute_rsp_t
 * @brief Sahara execute command response structure.
 *        Sahara 执行命令响应结构体。
 */
typedef struct {
    DWORD cmd;           // Command code / 命令码
    DWORD len;           // Length of the packet / 数据包长度
    DWORD client_cmd;    // Client command / 客户端命令
    DWORD data_len;      // Data length / 数据长度
} execute_rsp_t;

/**
 * @struct image_end_t
 * @brief Sahara image end structure.
 *        Sahara 镜像结束结构体。
 */
typedef struct {
    DWORD id;            // Image ID / 镜像 ID
    DWORD status;        // Status / 状态
} image_end_t;

/**
 * @struct done_t
 * @brief Sahara done structure.
 *        Sahara 完成结构体。
 */
typedef struct {
    DWORD cmd;           // Command code / 命令码
    DWORD len;           // Length of the packet / 数据包长度
    DWORD status;        // Status / 状态
} done_t;

/**
 * @struct pbl_info_t
 * @brief Sahara PBL (Primary Boot Loader) information structure.
 *        Sahara PBL（主引导加载程序）信息结构体。
 */
typedef struct {
    DWORD serial;        // Serial number / 序列号
    DWORD msm_id;        // MSM ID / MSM ID
    BYTE pk_hash[32];    // OEM public key hash / OEM 公钥哈希
    DWORD pbl_sw;        // PBL software version / PBL 软件版本
} pbl_info_t;

/**
 * @class Sahara
 * @brief Sahara protocol implementation for Qualcomm devices.
 *        高通设备 Sahara 协议实现。
 *
 * This class provides functionality to communicate with Qualcomm devices
 * using the Sahara protocol for flashing and device management.
 * 本类提供使用 Sahara 协议与高通设备通信的功能，用于刷机和设备管理。
 */
class Sahara {
public:
    /**
     * @brief Constructor.
     *        构造函数。
     * @param port [in] Serial port pointer for communication. 用于通信的串口指针。
     * @param hLogFile [in] Handle to the log file (optional). 日志文件句柄（可选）。
     */
    Sahara(SerialPort* port, HANDLE hLogFile = NULL);
    
    /**
     * @brief Reset the device.
     *        重置设备。
     * @return Status code. 错误代码。
     */
    int DeviceReset(void);
    
    /**
     * @brief Load flash program file (modern version).
     *        加载烧录程序文件（现代版本）。
     * @param szFlashPrg [in] Path to the flash program file. 烧录程序文件路径。
     * @return Status code. 错误代码。
     */
    int LoadFlashProg(const std::string& szFlashPrg);
    
    /**
     * @brief Connect to the device.
     *        连接到设备。
     * @param bReadHello [in] Whether to read hello response. 是否读取握手响应。
     * @param mode [in] Mode to connect in. 连接模式。
     * @return Status code. 错误代码。
     */
    int ConnectToDevice(bool bReadHello, int mode);
    
    /**
     * @brief Dump device information.
     *        转储设备信息。
     * @param pbl_info [out] Pointer to store PBL information. 存储 PBL 信息的指针。
     * @return Status code. 错误代码。
     */
    int DumpDeviceInfo(pbl_info_t* pbl_info);
    
    /**
     * @brief Check if device is available.
     *        检查设备是否可用。
     * @return True if device is available, false otherwise. 如果设备可用则返回 true，否则返回 false。
     */
    bool CheckDevice(void);

private:
    /**
     * @brief Switch to a different mode.
     *        切换到不同的模式。
     * @param mode [in] Mode to switch to. 要切换到的模式。
     * @return Status code. 错误代码。
     */
    int ModeSwitch(int mode);
    
    /**
     * @brief Convert hex string to byte array.
     *        将十六进制字符串转换为字节数组。
     * @param hex [in] Hex string to convert. 要转换的十六进制字符串。
     * @param bin [out] Byte array to store the result. 存储结果的字节数组。
     * @param len [in] Length of the hex string. 十六进制字符串的长度。
     */
    void HexToByte(const char* hex, BYTE* bin, int len);
    
    /**
     * @brief Convert hex string to byte array (modern version).
     *        将十六进制字符串转换为字节数组（现代版本）。
     * @param hex [in] Hex string to convert. 要转换的十六进制字符串。
     * @param bin [out] ByteArray to store the result. 存储结果的 ByteArray。
     */
    void HexToByte(const std::string& hex, ByteArray& bin);
    
    /**
     * @brief Log a message.
     *        记录日志消息。
     * @param str [in] Format string for the message. 消息的格式字符串。
     * @param ... [in] Variable arguments for the format string. 格式字符串的可变参数。
     */
    void Log(const char* str, ...);

    /**
     * @brief Get the run address from a hex file.
     *        从十六进制文件获取运行地址。
     * @param filename [in] Path to the hex file. 十六进制文件路径。
     * @return Run address. 运行地址。
     */
    DWORD HexRunAddress(std::string filename);
    
    /**
     * @brief Get the run address from a hex file (modern version).
     *        从十六进制文件获取运行地址（现代版本）。
     * @param filename [in] Path to the hex file. 十六进制文件路径。
     * @return Run address. 运行地址。
     */
    DWORD HexRunAddress(const std::string& filename);
    
    /**
     * @brief Get the data length from a hex file.
     *        从十六进制文件获取数据长度。
     * @param filename [in] Path to the hex file. 十六进制文件路径。
     * @return Data length. 数据长度。
     */
    DWORD HexDataLength(std::string filename);
    
    /**
     * @brief Get the data length from a hex file (modern version).
     *        从十六进制文件获取数据长度（现代版本）。
     * @param filename [in] Path to the hex file. 十六进制文件路径。
     * @return Data length. 数据长度。
     */
    DWORD HexDataLength(const std::string& filename);
    
    /**
     * @brief Read data from device.
     *        从设备读取数据。
     * @param cmd [in] Command code. 命令码。
     * @param buf [out] Buffer to store the read data. 存储读取数据的缓冲区。
     * @param len [in] Number of bytes to read. 要读取的字节数。
     * @return Status code. 错误代码。
     */
    int ReadData(int cmd, BYTE* buf, int len);
    
    /**
     * @brief Read data from device (modern version).
     *        从设备读取数据（现代版本）。
     * @param cmd [in] Command code. 命令码。
     * @param buf [out] ByteArray to store the read data. 存储读取数据的 ByteArray。
     * @param len [in] Number of bytes to read. 要读取的字节数。
     * @return Status code. 错误代码。
     */
    int ReadData(int cmd, ByteArray& buf, int len);
    
    /**
     * @brief Apply PBL hack.
     *        应用 PBL hack。
     * @return Status code. 错误代码。
     */
    int PblHack(void);

    SerialPort* sport;
    bool bSectorAddress;
    HANDLE hLog;
};
