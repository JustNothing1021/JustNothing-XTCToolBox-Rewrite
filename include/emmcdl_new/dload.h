/*****************************************************************************
 * dload.h
 *
 * This file implements the DLOAD protocol for Qualcomm devices
 * 本文件实现了高通设备的 DLOAD 协议
 *
 * Copyright (c) 2007-2013
 * Qualcomm Technologies Incorporated.
 * All Rights Reserved.
 * Qualcomm Confidential and Proprietary
 *
 *****************************************************************************/

#pragma once

#include "emmcdl_new/serialport.h"
#include "emmcdl_new/partition.h"
#include "datatypes/bytearray.h"
#include <stdio.h>
#include <Windows.h>
#include <string>

#define PACKET_TIMEOUT 1000

#define FEATURE_SECTOR_ADDRESSES   0x00000010

// Packets that are used in dload mode
// DLOAD 模式下使用的数据包
#define CMD_WRITE  0x01   /* Write a block of data to memory (received)    写入数据块到内存 */
#define CMD_ACK     0x02   /* Acknowledge receiving a packet  (transmitted) 确认接收到数据包 */
#define CMD_NAK     0x03   /* Acknowledge a bad packet        (transmitted) 确认接收到错误数据包 */
#define CMD_ERASE   0x04   /* Erase a block of memory         (received)    擦除内存块 */
#define CMD_GO      0x05   /* Begin execution at an address   (received)    从指定地址开始执行 */
#define CMD_NOP     0x06   /* No operation, for debug         (received)    空操作，用于调试 */
#define CMD_PREQ    0x07   /* Request implementation info     (received)    请求实现信息 */
#define CMD_PARAMS  0x08   /* Provide implementation info     (transmitted) 提供实现信息 */
#define CMD_DUMP    0x09   /* Debug: dump a block of memory   (received)    调试：转储内存块 */
#define CMD_RESET   0x0A   /* Reset phone                 (received)    重置手机 */
#define CMD_UNLOCK  0x0B   /* Unlock access to secured ops    (received)    解锁安全操作 */
#define CMD_VERREQ  0x0C   /* Request software version info   (received)    请求软件版本信息 */
#define CMD_VERRSP  0x0D   /* Provide software version info   (transmitted) 提供软件版本信息 */
#define CMD_PWROFF  0x0E   /* Turn phone power off            (received)    关闭手机电源 */
#define CMD_WRITE_32BIT  0x0F  /* Write a block of data to 32-bit memory 写入数据块到32位内存 */
#define CMD_MEM_DEBUG_QUERY  0x10 /* Memory debug query       (received)    内存调试查询 */
#define CMD_MEM_DEBUG_INFO  0x11 /* Memory debug info        (transmitted) 内存调试信息 */
#define CMD_MEM_READ_REQ     0x12 /* Memory read request      (received)    内存读取请求 */
#define CMD_MEM_READ_RESP    0x13 /* Memory read response     (transmitted) 内存读取响应 */
#define CMD_MEM_UNFRAMED_READ_REQ  0x14 /* Memory unframed read request   (received)      无帧内存读取请求 */
#define CMD_MEM_UNFRAMED_READ_RESP  0x15 /* Memory unframed read response  (transmitted)   无帧内存读取响应 */
#define CMD_SERIAL_NUM_READ   0x16 /* Return Serial number from efuse read   从 efuse 读取序列号 */
#define CMD_MSM_HW_ID_READ    0x17 /* Return msm_hw_id from efuse read   从 efuse 读取 msm_hw_id */
#define CMD_OEM_HASH_KEY_READ  0x18 /* Return oem_pk_hash from efuse read   从 efuse 读取 oem_pk_hash */
#define CMD_QPST_COOKIE_READ_REQ  0x19 /* QPST cookie read request      (received)    QPST cookie 读取请求 */
#define CMD_QPST_COOKIE_READ_RESP  0x1A /* QPST cookie read response     (transmitted) QPST cookie 读取响应 */      
#define CMD_DLOAD_SWITCH  0x3A

// Partition types
// 分区类型
#define PRTN_PBL           1  // don't change value
#define PRTN_QCSBLDHDR     2  // don't change value
#define PRTN_QCSBL         3  // don't change value
#define PRTN_OEMSBL        4  // don't change value
#define PRTN_MODEM         5  // don't change value
#define PRTN_APPS          6  // don't change value
#define PRTN_OTPBL         7  // don't change value
#define PRTN_FOTAUI        8	// don't change value
#define PRTN_CEFS          9  // Write CEFS to the modem f/s
#define PRTN_APPSBL       10  // don't change value
#define PRTN_CEFSAPPS     11  // Write CEFS to the apps f/s
#define PRTN_WINMOBILE    12  // Microsoft WinMobile image
#define PRTN_QDSP6        13  // QDSP6 executable
#define PRTN_USER         14  // User defined partition data
#define PRTN_DBL          15  // SB 2.0 Device boot loader
#define PRTN_OSBL         16  // SB 2.0 OS boot loader
#define PRTN_FSBL         17  // SB 2.0 Failsafe boot loader
#define PRTN_QDSP6_2      18  // dsp2.mbn
#define PRTN_RAWAPPSEFS   19  // Raw (includes ECC + spare data) 
#define PRTN_ROFS1        20  // 0x14 - Symbian
#define PRTN_ROFS2        21  // 0x15 - Symbian
#define PRTN_ROFS3        22  // 0x16 - Symbian
#define PRTN_MBR          32  // mbr.bin for NOR (boot) + MoviNand (AMSS+APPS)
#define PRTN_EMMCUSER     33  // For programming eMMC chip (singleimage.mbn)
#define PRTN_EMMCBOOT0    34  // BOOT and GPP partitions
#define PRTN_EMMCBOOT1    35
#define PRTN_EMMCRPMB     36
#define PRTN_EMMCGPP1     37
#define PRTN_EMMCGPP2     38
#define PRTN_EMMCGPP3     39
#define PRTN_EMMCGPP4     40

// Packets that are used in emergency download mode
// 紧急下载模式下使用的数据包
#define EHOST_HELLO_REQ       0x01  // Hello request
#define EHOST_HELLO_RSP       0x02  // Hello response
#define EHOST_SIMPLE_READ_REQ   0x03  // Read some number of bytes from phone memory
#define EHOST_SIMPLE_READ_RSP   0x04  // Response to simple read
#define EHOST_SIMPLE_WRITE_REQ  0x05  // Write data without erase
#define EHOST_SIMPLE_WRITE_RSP  0x06  // Response to simple write
#define EHOST_STREAM_WRITE_REQ  0x07  // Streaming write command
#define EHOST_STREAM_WRITE_RSP  0x08  // Response to stream write
#define EHOST_NOP_REQ           0x09  // NOP request
#define EHOST_NOP_RSP           0x0A  // NOP response
#define EHOST_RESET_REQ         0x0B  // Reset target
#define EHSOT_RESET_ACK         0x0C  // Response to reset
#define EHOST_ERROR             0x0D  // Target Error
#define EHOST_LOG               0x0E  // Target informational message
#define EHOST_UNLOCK_REQ        0x0F  // Unlock Target
#define EHOST_UNLOCK_RSP        0x10  // Target unlocked
#define EHOST_POWER_OFF_REQ     0x11  // Power off target
#define EHOST_POWER_OFF_RSP     0x12  // Power off target response
#define EHOST_OPEN_REQ          0x13  // Open for writing image
#define EHOST_OPEN_RSP          0x14  // Response to open for writing image
#define EHOST_CLOSE_REQ         0x15  // Close and flush last partial write to Flash
#define EHOST_CLOSE_RSP         0x16  // Response to close and flush last partial write to Flash
#define EHOST_SECURITY_REQ      0x17  // Send Security mode to use for programming images
#define EHOST_SECURITY_RSP      0x18  // Response to Send Security mode
#define EHOST_PARTITION_REQ     0x19  // Send partition table to use for programming images
#define EHOST_PARTITION_RSP     0x1A  // Response to send partition table
#define EHOST_OPEN_MULTI_REQ    0x1B  // Open for writing image (Multi-image mode only)
#define EHOST_OPEN_MULTI_RSP    0x1C  // Response to open for writing image
#define EHOST_ERASE_FLASH_REQ   0x1D  // Erase entire Flash device
#define EHOST_ERASE_FLASH_RSP   0x1E  // Response to erase Flash
#define EHOST_GET_ECC_REQ       0x1F  // Read current ECC generation status
#define EHOST_GET_ECC_RSP       0x20  // Response to Get ECC state with current status
#define EHOST_SET_ECC_REQ       0x21  // Enable/disable hardware ECC generation
#define EHOST_SET_ECC_RSP       0x22  // Response to set ECC
#define EHOST_STREAM_DLOAD_REQ  0x23  // New streaming download request
#define EHOST_STREAM_DLOAD_RSP  0x24  // New streaming download response
#define EHOST_UNFRAMED_REQ      0x30  // Unframed streaming write command
#define EHOST_UNFRAMED_RSP      0x31  // Unframed streaming write response


/**
 * @class Dload
 * @brief DLOAD protocol implementation for Qualcomm devices.
 *        高通设备 DLOAD 协议实现。
 *
 * This class provides functionality to communicate with Qualcomm devices
 * using the DLOAD protocol for flashing and device management.
 * 本类提供使用 DLOAD 协议与高通设备通信的功能，用于刷机和设备管理。
 */
class Dload {
public:
    /**
     * @brief Constructor.
     *        构造函数。
     * @param port [in] Serial port pointer for communication. 用于通信的串口指针。
     */
    Dload(SerialPort* port);
    
    /**
     * @brief Connect to flash programmer.
     *        连接到烧录程序。
     * @param ver [in] Version byte. 版本字节。
     * @return Status code. 错误代码。
     */
    int ConnectToFlashProg(BYTE ver);
    
    /**
     * @brief Reset the device.
     *        重置设备。
     * @return Status code. 错误代码。
     */
    int DeviceReset(void);
    
    /**
     * @brief Load a partition from file.
     *        从文件加载分区。
     * @param filename [in] Path to the partition file. 分区文件路径。
     * @return Status code. 错误代码。
     */
    int LoadPartition(std::string filename);
    
    /**
     * @brief Load a partition from file (modern version).
     *        从文件加载分区（现代版本）。
     * @param filename [in] Path to the partition file. 分区文件路径。
     * @return Status code. 错误代码。
     */
    int LoadPartition(const std::string& filename);
    
    /**
     * @brief Load an image file (modern version).
     *        加载镜像文件（现代版本）。
     * @param szSingleImage [in] Path to the image file. 镜像文件路径。
     * @return Status code. 错误代码。
     */
    int LoadImageFile(const std::string& szSingleImage);
    
    /**
     * @brief Create GPP (General Purpose Partition) partitions.
     *        创建通用分区（GPP）。
     * @param dwGPP1 [in] Size of GPP1 in KB. GPP1 大小（KB）。
     * @param dwGPP2 [in] Size of GPP2 in KB. GPP2 大小（KB）。
     * @param dwGPP3 [in] Size of GPP3 in KB. GPP3 大小（KB）。
     * @param dwGPP4 [in] Size of GPP4 in KB. GPP4 大小（KB）。
     * @return Status code. 错误代码。
     */
    int CreateGPP(uint32_t dwGPP1, uint32_t dwGPP2, uint32_t dwGPP3, uint32_t dwGPP4);
    
    /**
     * @brief Set the active partition.
     *        设置活动分区。
     * @return Status code. 错误代码。
     */
    int SetActivePartition();
    
    /**
     * @brief Open a partition for operations.
     *        打开分区以进行操作。
     * @param partition [in] Partition number to open. 要打开的分区号。
     * @return Status code. 错误代码。
     */
    int OpenPartition(int partition);
    
    /**
     * @brief Close the currently open partition.
     *        关闭当前打开的分区。
     * @return Status code. 错误代码。
     */
    int ClosePartition();
    
    /**
     * @brief Fast copy from serial port to disk.
     *        从串口快速复制到磁盘。
     * @param hInFile [in] Input file handle. 输入文件句柄。
     * @param sector [in] Starting sector. 起始扇区。
     * @param sectors [in] Number of sectors to copy. 要复制的扇区数。
     * @return Status code. 错误代码。
     */
    int FastCopySerial(HANDLE hInFile, uint32_t sector, uint32_t sectors);
    
    /**
     * @brief Load flash program file (modern version).
     *        加载烧录程序文件（现代版本）。
     * @param szFlashPrg [in] Path to the flash program file. 烧录程序文件路径。
     * @return Status code. 错误代码。
     */
    int LoadFlashProg(const std::string& szFlashPrg);
    
    /**
     * @brief Write raw program file to device (modern version).
     *        将原始程序文件写入设备（现代版本）。
     * @param szXMLFile [in] Path to the XML file. XML 文件路径。
     * @return Status code. 错误代码。
     */
    int WriteRawProgramFile(const std::string& szXMLFile);
    
    /**
     * @brief Get DLOAD parameters.
     *        获取 DLOAD 参数。
     * @param rsp [out] Buffer to store the response. 存储响应的缓冲区。
     * @param len [in] Length of the buffer. 缓冲区长度。
     * @return Status code. 错误代码。
     */
    int GetDloadParams(BYTE* rsp, int len);
    
    /**
     * @brief Get DLOAD parameters (modern version).
     *        获取 DLOAD 参数（现代版本）。
     * @param rsp [out] ByteArray to store the response. 存储响应的 ByteArray。
     * @return Status code. 错误代码。
     */
    int GetDloadParams(ByteArray& rsp);
    
    /**
     * @brief Check if device is in DLOAD mode.
     *        检查设备是否处于 DLOAD 模式。
     * @return True if device is in DLOAD mode, false otherwise. 如果设备处于 DLOAD 模式则返回 true，否则返回 false。
     */
    int IsDeviceInDload(void);
    
    /**
     * @brief Wipe disk contents from specified sector.
     *        从指定扇区擦除磁盘内容。
     * @param start_sector [in] Starting sector number. 起始扇区号。
     * @param num_sectors [in] Number of sectors to wipe. 要擦除的扇区数。
     * @return Status code. 错误代码。
     */
    int WipeDiskContents(uint64_t start_sector, uint64_t num_sectors);

private:
    /**
     * @brief Convert hex string to byte array.
     *        将十六进制字符串转换为字节数组。
     * @param hex [in] Hex string to convert. 要转换的十六进制字符串。
     * @param bin [out] Byte array to store the result. 存储结果的字节数组。
     * @param len [in] Length of the hex string. 十六进制字符串的长度。
     */
    void HexTobyte(const char* hex, BYTE* bin, int len);
    
    /**
     * @brief Convert hex string to byte array (modern version).
     *        将十六进制字符串转换为字节数组（现代版本）。
     * @param hex [in] Hex string to convert. 要转换的十六进制字符串。
     * @param bin [out] ByteArray to store the result. 存储结果的 ByteArray。
     */
    void HexTobyte(const std::string& hex, ByteArray& bin);
    
    /**
     * @brief Get the number of disk sectors.
     *        获取磁盘扇区数。
     * @return Number of disk sectors. 磁盘扇区数。
     */
    uint64_t GetNumDiskSectors();
    
    /**
     * @brief Program a partition entry.
     *        编程分区条目。
     * @param pe [in] Partition entry to program. 要编程的分区条目。
     * @return Status code. 错误代码。
     */
    int ProgramPartitionEntry(PartitionEntry pe);

    SerialPort* sport;
    bool bSectorAddress;
};
