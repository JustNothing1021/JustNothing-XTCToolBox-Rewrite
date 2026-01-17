/*****************************************************************************
 * diskwriter.h
 *
 * This file implements direct disk access for local storage operations
 * 本文件实现了本地存储操作的直接磁盘访问
 *
 * This module provides functionality to directly access and write to
 * physical disks on the local system, bypassing the file system.
 * It handles disk enumeration, volume management, and low-level I/O operations.
 * 此模块提供直接访问和写入本地系统物理磁盘的功能，
 * 绕过文件系统。它处理磁盘枚举、卷管理和低级 I/O 操作。
 *
 *****************************************************************************/

#pragma once

#include "emmcdl_new/partition.h"
#include "emmcdl_new/protocol.h"
#include "datatypes/bytearray.h"
#include <windows.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <winerror.h>
#include <winioctl.h>
#include <stdio.h>
#include <tchar.h>

#define WRITE_TIMEOUT_MS    3000  // Write timeout in milliseconds / 写入超时（毫秒）
#define MAX_VOLUMES         26     // Maximum number of volumes / 最大卷数
#define MAX_DISKS           26     // Maximum number of disks / 最大磁盘数

/**
 * @struct vol_entry_t
 * @brief Volume entry structure.
 *        卷条目结构体。
 *
 * This structure contains information about a volume on the system.
 * 此结构体包含系统上卷的信息。
 */
typedef struct {
    int32_t     serialnum;    // Volume serial number / 卷序列号
    int32_t      drivetype;    // Drive type / 驱动器类型
    uint64_t     volsize;       // Volume size / 卷大小
    int          disknum;       // Disk number / 磁盘号
    char        fstype[MAX_PATH + 1];  // File system type / 文件系统类型
    char        mount[MAX_PATH + 1];    // Mount point / 挂载点
    char        rootpath[MAX_PATH + 1]; // Root path / 根路径
    char        volume[MAX_PATH + 1];   // Volume name / 卷名
} vol_entry_t;

/**
 * @struct disk_entry_t
 * @brief Disk entry structure.
 *        磁盘条目结构体。
 *
 * This structure contains information about a physical disk on the system.
 * 此结构体包含系统上物理磁盘的信息。
 */
typedef struct {
    uint64_t     disksize;       // Disk size / 磁盘大小
    int          blocksize;      // Block size / 块大小
    int          disknum;        // Disk number / 磁盘号
    int          volnum[MAX_VOLUMES + 1]; // Volume numbers / 卷号
    char        diskname[MAX_PATH + 1];   // Disk name / 磁盘名
} disk_entry_t;

/**
 * @class DiskWriter
 * @brief Direct disk access handler.
 *        直接磁盘访问处理器。
 *
 * This class provides functionality to directly access and write to
 * physical disks on the local system. It handles disk enumeration,
 * volume management, and low-level I/O operations.
 * 本类提供直接访问和写入本地系统物理磁盘的功能。
 * 它处理磁盘枚举、卷管理和低级 I/O 操作。
 */
class DiskWriter : public Protocol {
public:
    int diskcount;     // Number of disks / 磁盘数量
    bool bPatchDisk;   // Flag for patch disk / 补丁磁盘标志

    /**
     * @brief Constructor.
     *        构造函数。
     */
    DiskWriter();
    
    /**
     * @brief Destructor.
     *        析构函数。
     */
    ~DiskWriter();

    /**
     * @brief Write data to disk.
     *        将数据写入磁盘。
     * @param writeBuffer [in] Buffer containing data to write. 包含要写入数据的缓冲区。
     * @param writeOffset [in] Offset to write to (in sectors). 写入偏移量（扇区）。
     * @param writeBytes [in] Number of bytes to write. 要写入的字节数。
     * @param bytesWritten [out] Number of bytes actually written. 实际写入的字节数。
     * @param partNum [in] Partition number. 分区号。
     * @return Status code. 错误代码。
     */
    int WriteData(BYTE* writeBuffer, int64_t writeOffset, DWORD writeBytes, DWORD* bytesWritten, uint8_t partNum);
    
    /**
     * @brief Write data to disk (modern version).
     *        将数据写入磁盘（现代版本）。
     * @param writeBuffer [in] ByteArray containing data to write. 包含要写入数据的 ByteArray。
     * @param writeOffset [in] Offset to write to (in sectors). 写入偏移量（扇区）。
     * @param bytesWritten [out] Number of bytes actually written. 实际写入的字节数。
     * @param partNum [in] Partition number. 分区号。
     * @return Status code. 错误代码。
     */
    int WriteData(const ByteArray& writeBuffer, int64_t writeOffset, DWORD* bytesWritten, uint8_t partNum);
    
    /**
     * @brief Read data from disk.
     *        从磁盘读取数据。
     * @param readBuffer [out] Buffer to store read data. 存储读取数据的缓冲区。
     * @param readOffset [in] Offset to read from (in sectors). 读取偏移量（扇区）。
     * @param readBytes [in] Number of bytes to read. 要读取的字节数。
     * @param bytesRead [out] Number of bytes actually read. 实际读取的字节数。
     * @param partNum [in] Partition number. 分区号。
     * @return Status code. 错误代码。
     */
    int ReadData(BYTE* readBuffer, int64_t readOffset, DWORD readBytes, DWORD* bytesRead, uint8_t partNum);
    
    /**
     * @brief Read data from disk (modern version).
     *        从磁盘读取数据（现代版本）。
     * @param readBuffer [out] ByteArray to store read data. 存储读取数据的 ByteArray。
     * @param readOffset [in] Offset to read from (in sectors). 读取偏移量（扇区）。
     * @param readBytes [in] Number of bytes to read. 要读取的字节数。
     * @param bytesRead [out] Number of bytes actually read. 实际读取的字节数。
     * @param partNum [in] Partition number. 分区号。
     * @return Status code. 错误代码。
     */
    int ReadData(ByteArray& readBuffer, int64_t readOffset, DWORD readBytes, DWORD* bytesRead, uint8_t partNum);

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
    int FastCopy(HANDLE hRead, int64_t sectorRead, HANDLE hWrite, int64_t sectorWrite, uint64_t sectors, uint8_t partNum = 0);
    
    /**
     * @brief Open a device by disk number.
     *        通过磁盘号打开设备。
     * @param dn [in] Disk number. 磁盘号。
     * @return Status code. 错误代码。
     */
    int OpenDevice(int dn);
    
    /**
     * @brief Open a disk file.
     *        打开磁盘文件。
     * @param oFile [in] File path. 文件路径。
     * @param sectors [in] Number of sectors. 扇区数。
     * @return Status code. 错误代码。
     */
    int OpenDiskFile(const char* oFile, uint64_t sectors);
    
    /**
     * @brief Close the device.
     *        关闭设备。
     */
    void CloseDevice();
    
    /**
     * @brief Initialize disk list.
     *        初始化磁盘列表。
     * @param verbose [in] Verbose output flag. 详细输出标志。
     * @return Status code. 错误代码。
     */
    int InitDiskList(bool verbose = false);
    
    /**
     * @brief Reset the device.
     *        重置设备。
     * @return Status code. 错误代码。
     */
    int DeviceReset(void);
    
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
     * @brief Corruption test.
     *        损坏测试。
     * @param offset [in] Offset for test. 测试偏移量。
     * @return Status code. 错误代码。
     */
    int CorruptionTest(uint64_t offset);
    
    /**
     * @brief Disk test.
     *        磁盘测试。
     * @param offset [in] Offset for test. 测试偏移量。
     * @return Status code. 错误代码。
     */
    int DiskTest(uint64_t offset);
    
    /**
     * @brief Wipe disk layout.
     *        擦除磁盘布局。
     * @return Status code. 错误代码。
     */
    int WipeLayout();

protected:

private:
    HANDLE hVolume;         // Volume handle / 卷句柄
    int disk_num;          // Disk number / 磁盘号
    HANDLE hFS;            // File system handle / 文件系统句柄
    OVERLAPPED ovl;       // Overlapped structure for async I/O / 异步 I/O 的重叠结构体
    disk_entry_t* disks;   // Array of disk entries / 磁盘条目数组
    vol_entry_t* volumes;   // Array of volume entries / 卷条目数组

    /**
     * @brief Get volume information.
     *        获取卷信息。
     * @param vol [in] Volume entry. 卷条目。
     * @return Status code. 错误代码。
     */
    int GetVolumeInfo(vol_entry_t* vol);
    
    /**
     * @brief Get disk information.
     *        获取磁盘信息。
     * @param de [in] Disk entry. 磁盘条目。
     * @return Status code. 错误代码。
     */
    int GetDiskInfo(disk_entry_t* de);
    
    /**
     * @brief Convert TCHAR to string.
     *        将 TCHAR 转换为字符串。
     * @param p [in] TCHAR pointer. TCHAR 指针。
     * @return Converted string. 转换后的字符串。
     */
    TCHAR* TCharToString(TCHAR* p);
    
    /**
     * @brief Unmount a volume.
     *        卸载卷。
     * @param vol [in] Volume entry. 卷条目。
     * @return Status code. 错误代码。
     */
    int UnmountVolume(vol_entry_t vol);
    
    /**
     * @brief Lock the device.
     *        锁定设备。
     * @return Status code. 错误代码。
     */
    int LockDevice();
    
    /**
     * @brief Unlock the device.
     *        解锁设备。
     * @return Status code. 错误代码。
     */
    int UnlockDevice();
    
    /**
     * @brief Check if device is writeable.
     *        检查设备是否可写。
     * @return True if writeable, false otherwise. 如果可写则返回 true，否则返回 false。
     */
    bool IsDeviceWriteable();
    
    /**
     * @brief Get raw disk size.
     *        获取原始磁盘大小。
     * @param ds [out] Disk size. 磁盘大小。
     * @return Status code. 错误代码。
     */
    int GetRawDiskSize(uint64_t* ds);
    
    /**
     * @brief Raw read test.
     *        原始读取测试。
     * @param offset [in] Offset for test. 测试偏移量。
     * @return Status code. 错误代码。
     */
    int RawReadTest(uint64_t offset);
};
