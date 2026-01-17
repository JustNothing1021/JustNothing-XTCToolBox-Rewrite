/*****************************************************************************
 * protocol.h
 *
 * This file defines the abstract protocol base class for device communication
 * 本文件定义了设备通信的抽象协议基类
 *
 * This module provides an abstract base class for implementing different
 * communication protocols (Firehose, Sahara, etc.) for Qualcomm devices.
 * It defines common interfaces for data transfer, device management,
 * and partition operations.
 * 此模块提供用于实现高通设备的不同通信协议（Firehose、Sahara 等）
 * 的抽象基类。它定义了数据传输、设备管理和分区操作的通用接口。
 *
 *****************************************************************************/

#pragma once

#include "emmcdl_new/partition.h"
#include "datatypes/bytearray.h"
#include <string>
#include <vector>
#include <windows.h>

#define MAX_XML_LEN         2048  // Maximum XML length / 最大 XML 长度
#define MAX_TRANSFER_SIZE   0x100000  // Maximum transfer size / 最大传输大小

/**
 * @class Protocol
 * @brief Abstract protocol base class for device communication.
 *        设备通信的抽象协议基类。
 *
 * This class provides an abstract interface for implementing different
 * communication protocols. It defines common methods for data transfer,
 * device management, and partition operations that must be implemented
 * by derived classes.
 * 本类提供用于实现不同通信协议的抽象接口。它定义了数据传输、
 * 设备管理和分区操作的通用方法，这些方法必须由派生类实现。
 */
class Protocol {
public:

    /**
     * @brief Constructor.
     *        构造函数。
     */
    Protocol();
    
    /**
     * @brief Destructor.
     *        析构函数。
     */
    ~Protocol();

    /**
     * @brief Dump disk contents to file by sector range.
     *        按扇区范围将磁盘内容转储到文件。
     * @param start_sector [in] Starting sector. 起始扇区。
     * @param num_sectors [in] Number of sectors to dump. 要转储的扇区数。
     * @param szOutFile [in] Output file path. 输出文件路径。
     * @param partNum [in] Partition number. 分区号。
     * @param szPartName [in] Partition name (optional). 分区名称（可选）。
     * @return Status code. 错误代码。
     */
    int DumpDiskContents(uint64_t start_sector, uint64_t num_sectors, std::string szOutFile, uint8_t partNum, const std::string& szPartName = "");
    
    /**
     * @brief Dump disk contents to file by partition name.
     *        按分区名称将磁盘内容转储到文件。
     * @param szPartName [in] Partition name. 分区名称。
     * @param szOutFile [in] Output file path. 输出文件路径。
     * @param partNum [in] Partition number. 分区号。
     * @return Status code. 错误代码。
     */
    int DumpDiskContents(const std::string& szPartName, const std::string& szOutFile, uint8_t partNum);
    
    /**
     * @brief Wipe disk contents by sector range.
     *        按扇区范围擦除磁盘内容。
     * @param start_sector [in] Starting sector. 起始扇区。
     * @param num_sectors [in] Number of sectors to wipe. 要擦除的扇区数。
     * @param szPartName [in] Partition name (optional). 分区名称（可选）。
     * @return Status code. 错误代码。
     */
    int WipeDiskContents(uint64_t start_sector, uint64_t num_sectors, const std::string& szPartName = "");
    
    /**
     * @brief Wipe disk contents by partition name.
     *        按分区名称擦除磁盘内容。
     * @param szPartName [in] Partition name. 分区名称。
     * @return Status code. 错误代码。
     */
    int WipeDiskContents(const std::string& szPartName);

    /**
     * @brief Read GPT (GUID Partition Table) information.
     *        读取 GPT（GUID 分区表）信息。
     * @param show_result [in] Whether to show results in log. 是否在日志中显示结果。
     * @return Status code. 错误代码。
     */
    int ReadGPT(bool show_result);
    
    /**
     * @brief Write GPT (GUID Partition Table) to device.
     *        将 GPT（GUID 分区表）写入设备。
     * @param szPartName [in] Partition name. 分区名称。
     * @param szBinFile [in] Binary file path containing GPT data. 包含 GPT 数据的二进制文件路径。
     * @return Status code. 错误代码。
     */
    int WriteGPT(std::string szPartName, std::string szBinFile);
    
    /**
     * @brief Get disk sector size.
     *        获取磁盘扇区大小。
     * @return Sector size in bytes. 扇区大小（字节）。
     */
    int GetDiskSectorSize(void);
    
    /**
     * @brief Set disk sector size.
     *        设置磁盘扇区大小。
     * @param size [in] Sector size in bytes. 扇区大小（字节）。
     */
    void SetDiskSectorSize(int size);
    
    /**
     * @brief Get number of disk sectors.
     *        获取磁盘扇区数量。
     * @return Number of sectors. 扇区数量。
     */
    uint64_t GetNumDiskSectors(void);
    
    /**
     * @brief Get disk handle.
     *        获取磁盘句柄。
     * @return Disk handle. 磁盘句柄。
     */
    HANDLE GetDiskHandle(void);
    
    /**
     * @brief Enable verbose output.
     *        启用详细输出。
     */
    void EnableVerbose(void);

    /**
     * @brief Reset the device.
     *        重置设备。
     * @return Status code. 错误代码。
     */
    virtual int DeviceReset(void) = 0;
    
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
    virtual int WriteData(BYTE* writeBuffer, int64_t writeOffset, DWORD writeBytes, DWORD* bytesWritten, uint8_t partNum) = 0;
    
    /**
     * @brief Write data to device (modern version).
     *        将数据写入设备（现代版本）。
     * @param writeBuffer [in] ByteArray containing data to write. 包含要写入数据的 ByteArray。
     * @param writeOffset [in] Offset to write to (in sectors). 写入偏移量（扇区）。
     * @param bytesWritten [out] Number of bytes actually written. 实际写入的字节数。
     * @param partNum [in] Partition number. 分区号。
     * @return Status code. 错误代码。
     */
    virtual int WriteData(const ByteArray& writeBuffer, int64_t writeOffset, DWORD* bytesWritten, uint8_t partNum) = 0;
    
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
    virtual int ReadData(BYTE* readBuffer, int64_t readOffset, DWORD readBytes, DWORD* bytesRead, uint8_t partNum) = 0;
    
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
    virtual int ReadData(ByteArray& readBuffer, int64_t readOffset, DWORD readBytes, DWORD* bytesRead, uint8_t partNum) = 0;
    
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
    virtual int FastCopy(HANDLE hRead, int64_t sectorRead, HANDLE hWrite, int64_t sectorWrite, uint64_t sectors, uint8_t partNum) = 0;
    
    /**
     * @brief Program a raw command.
     *        编程原始命令。
     * @param cmd [in] Command string. 命令字符串。
     * @return Status code. 错误代码。
     */
    virtual int ProgramRawCommand(const std::string& cmd) = 0;
    
    /**
     * @brief Program a patch entry.
     *        编程补丁条目。
     * @param pe [in] Partition entry. 分区条目。
     * @param line [in] Line from XML file. XML 文件中的行。
     * @return Status code. 错误代码。
     */
    virtual int ProgramPatchEntry(PartitionEntry pe, const std::string& line) = 0;

protected:

    /**
     * @brief Load partition information into PartitionEntry structure.
     *        将分区信息加载到 PartitionEntry 结构体中。
     * @param szPartName [in] Partition name. 分区名称。
     * @param pEntry [out] Partition entry structure. 分区条目结构体。
     * @return Status code. 错误代码。
     */
    int LoadPartitionInfo(std::string szPartName, PartitionEntry* pEntry);

    gpt_header_t gpt_header;      // GPT header / GPT 头
    gpt_entry_t* gpt_entries;      // GPT entries array / GPT 条目数组
    uint64_t disk_size;            // Disk size / 磁盘大小
    HANDLE hDisk;                  // Disk handle / 磁盘句柄
    BYTE* buffer1;                // Buffer 1 / 缓冲区 1
    BYTE* buffer2;                // Buffer 2 / 缓冲区 2
    BYTE* bufAlloc1;              // Allocated buffer 1 / 分配的缓冲区 1
    BYTE* bufAlloc2;              // Allocated buffer 2 / 分配的缓冲区 2
    int DISK_SECTOR_SIZE;          // Disk sector size / 磁盘扇区大小
    bool bVerbose;                // Verbose output flag / 详细输出标志

};
