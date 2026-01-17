/*****************************************************************************
 * firehose.h
 *
 * This file implements the Firehose protocol for Qualcomm devices
 * 本文件实现了高通设备的 Firehose 协议
 *
 * Firehose is a high-speed streaming protocol used for flashing
 * Qualcomm devices. It provides efficient data transfer and
 * device management capabilities.
 * Firehose 是用于刷写高通设备的高速流式协议。它提供
 * 高效的数据传输和设备管理功能。
 *
 * Copyright (c) 2007-2013
 * Qualcomm Technologies Incorporated.
 * All Rights Reserved.
 * Qualcomm Confidential and Proprietary
 *
 *****************************************************************************/
 /*=============================================================================
                         Edit History

 $Header: //source/qcom/qct/platform/uefi/workspaces/pweber/apps/8x26_emmcdl/emmcdl/main/latest/inc/firehose.h#11 $
 $DateTime: 2015/04/01 17:01:45 $ $Author: pweber $

 when       who     what, where, why
 -------------------------------------------------------------------------------
 02/06/13   pgw     Initial version.
 =============================================================================*/
#pragma once

#include "serialport.h"
#include "protocol.h"
#include "partition.h"
#include "datatypes/bytearray.h"
#include <stdio.h>
#include <Windows.h>

#define MAX_RETRY   50  // Maximum retry count / 最大重试次数

/**
 * @struct fh_configure_t
 * @brief Firehose configuration structure.
 *        Firehose 配置结构体。
 *
 * This structure contains configuration parameters for Firehose protocol.
 * 此结构体包含 Firehose 协议的配置参数。
 */
typedef struct {
    BYTE Version;                           // Protocol version / 协议版本
    char MemoryName[8];                      // Memory name / 内存名称
    bool SkipWrite;                          // Skip write flag / 跳过写入标志
    bool SkipStorageInit;                     // Skip storage initialization flag / 跳过存储初始化标志
    bool ZLPAwareHost;                      // ZLP aware host flag / ZLP 感知主机标志
    int ActivePartition;                      // Active partition number / 活动分区号
    int MaxPayloadSizeToTargetInBytes;       // Maximum payload size to target in bytes / 到目标的最大载荷大小（字节）
} fh_configure_t;

/**
 * @class Firehose
 * @brief Firehose protocol implementation for Qualcomm devices.
 *        高通设备 Firehose 协议实现。
 *
 * This class provides functionality to communicate with Qualcomm devices
 * using the Firehose protocol for high-speed flashing and device management.
 * 本类提供使用 Firehose 协议与高通设备通信的功能，
 * 用于高速刷机和设备管理。
 */
class Firehose : public Protocol {
public:
    /**
     * @brief Constructor.
     *        构造函数。
     * @param port [in] Serial port pointer for communication. 用于通信的串口指针。
     * @param hLogFile [in] Handle to the log file (optional). 日志文件句柄（可选）。
     */
    Firehose(SerialPort* port, HANDLE hLogFile = NULL);
    
    /**
     * @brief Destructor.
     *        析构函数。
     */
    ~Firehose();

    /**
     * @brief Write data to device.
     *        将数据写入设备。
     * @param writeBuffer [in] Buffer containing data to write. 包含要写入数据的缓冲区。
     * @param writeOffset [in] Offset to write to (in sectors). 写入偏移量（扇区）。
     * @param writeBytes [in] Number of bytes to write. 要写入的字节数。
     * @param bytesWritten [out] Number of bytes actually written. 实际写入的字节数。
     * @param partNum [in] Partition number. 分区号。
     * @return Status code. 错误代码。
     */
    int WriteData(BYTE* writeBuffer, int64_t writeOffset, DWORD writeBytes, DWORD* bytesWritten, uint8_t partNum);
    
    /**
     * @brief Write data to device (modern version).
     *        将数据写入设备（现代版本）。
     * @param writeBuffer [in] ByteArray containing data to write. 包含要写入数据的 ByteArray。
     * @param writeOffset [in] Offset to write to (in sectors). 写入偏移量（扇区）。
     * @param bytesWritten [out] Number of bytes actually written. 实际写入的字节数。
     * @param partNum [in] Partition number. 分区号。
     * @return Status code. 错误代码。
     */
    int WriteData(const ByteArray& writeBuffer, int64_t writeOffset, DWORD* bytesWritten, uint8_t partNum);
    
    /**
     * @brief Read data from device.
     *        从设备读取数据。
     * @param readBuffer [out] Buffer to store read data. 存储读取数据的缓冲区。
     * @param readOffset [in] Offset to read from (in sectors). 读取偏移量（扇区）。
     * @param readBytes [in] Number of bytes to read. 要读取的字节数。
     * @param bytesRead [out] Number of bytes actually read. 实际读取的字节数。
     * @param partNum [in] Partition number. 分区号。
     * @return Status code. 错误代码。
     */
    int ReadData(BYTE* readBuffer, int64_t readOffset, DWORD readBytes, DWORD* bytesRead, uint8_t partNum);
    
    /**
     * @brief Read data from device (modern version).
     *        从设备读取数据（现代版本）。
     * @param readBuffer [out] ByteArray to store read data. 存储读取数据的 ByteArray。
     * @param readOffset [in] Offset to read from (in sectors). 读取偏移量（扇区）。
     * @param readBytes [in] Number of bytes to read. 要读取的字节数。
     * @param bytesRead [out] Number of bytes actually read. 实际读取的字节数。
     * @param partNum [in] Partition number. 分区号。
     * @return Status code. 错误代码。
     */
    int ReadData(ByteArray& readBuffer, int64_t readOffset, DWORD readBytes, DWORD* bytesRead, uint8_t partNum);

    /**
     * @brief Reset the device.
     *        重置设备。
     * @return Status code. 错误代码。
     */
    int DeviceReset(void);
    
    /**
     * @brief Fast copy between handles.
     *        在句柄之间快速复制。
     * @param hRead [in] Read handle. 读取句柄。
     * @param sectorRead [in] Read sector offset. 读取扇区偏移量。
     * @param hWrite [in] Write handle. 写入句柄。
     * @param sectorWrite [in] Write sector offset. 写入扇区偏移量。
     * @param sectors [in] Number of sectors to copy. 要复制的扇区数。
     * @param partNum [in] Partition number. 分区号。
     * @return Status code. 错误代码。
     */
    int FastCopy(HANDLE hRead, int64_t sectorRead, HANDLE hWrite, int64_t sectorWrite, uint64_t sectors, uint8_t partNum);
    
    /**
     * @brief Program a patch entry.
     *        编程补丁条目。
     * @param pe [in] Partition entry. 分区条目。
     * @param key [in] Key from XML file. XML 文件中的键。
     * @return Status code. 错误代码。
     */
    int ProgramPatchEntry(PartitionEntry pe, const std::string& key);
    
    /**
     * @brief Program a raw command.
     *        编程原始命令。
     * @param key [in] Key from XML file. XML 文件中的键。
     * @return Status code. 错误代码。
     */
    int ProgramRawCommand(const std::string& key);

    /**
     * @brief Create GPP (General Purpose Partition) partitions.
     *        创建通用分区（GPP）。
     * @param dwGPP1 [in] Size of GPP1 in KB. GPP1 大小（KB）。
     * @param dwGPP2 [in] Size of GPP2 in KB. GPP2 大小（KB）。
     * @param dwGPP3 [in] Size of GPP3 in KB. GPP3 大小（KB）。
     * @param dwGPP4 [in] Size of GPP4 in KB. GPP4 大小（KB）。
     * @return Status code. 错误代码。
     */
    int CreateGPP(DWORD dwGPP1, DWORD dwGPP2, DWORD dwGPP3, DWORD dwGPP4);
    
    /**
     * @brief Set the active partition.
     *        设置活动分区。
     * @param prtn_num [in] Partition number to set as active. 要设置为活动的分区号。
     * @return Status code. 错误代码。
     */
    int SetActivePartition(int prtn_num);
    
    /**
     * @brief Connect to flash programmer.
     *        连接到烧录程序。
     * @param cfg [in] Firehose configuration. Firehose 配置。
     * @return Status code. 错误代码。
     */
    int ConnectToFlashProg(fh_configure_t* cfg);

protected:

private:
    /**
     * @brief Read data from device.
     *        从设备读取数据。
     * @param pOutBuf [out] Output buffer. 输出缓冲区。
     * @param uiBufSize [in] Buffer size. 缓冲区大小。
     * @param bXML [in] XML flag. XML 标志。
     * @return Status code. 错误代码。
     */
    int ReadData(BYTE* pOutBuf, DWORD uiBufSize, bool bXML);
    
    /**
     * @brief Read status from device.
     *        从设备读取状态。
     * @return Status code. 错误代码。
     */
    int ReadStatus(void);

    SerialPort* sport;              // Serial port pointer / 串口指针
    uint64_t diskSectors;          // Disk sectors / 磁盘扇区数
    bool bSectorAddress;            // Sector address flag / 扇区地址标志
    BYTE* m_payload;               // Payload buffer / 载荷缓冲区
    BYTE* m_buffer;                // Buffer / 缓冲区
    BYTE* m_buffer_ptr;            // Buffer pointer / 缓冲区指针
    DWORD m_buffer_len;            // Buffer length / 缓冲区长度
    uint32_t dwMaxPacketSize;       // Maximum packet size / 最大数据包大小
    HANDLE hLog;                   // Log file handle / 日志文件句柄
    char* program_pkt;             // Program packet / 编程数据包
};
