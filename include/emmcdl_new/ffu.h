/*****************************************************************************
 * ffu.h
 *
 * This file has structures and functions for FFU (Full Flash Update) format
 * 本文件包含 FFU（完整固件更新）格式的结构和函数
 *
 * FFU format is used by Windows Phone/Windows Mobile devices for
 * full system image flashing. It contains security headers, image metadata,
 * and partition information for complete device updates.
 * FFU 格式由 Windows Phone/Windows Mobile 设备用于完整系统镜像刷写。
 * 它包含安全头、镜像元数据和分区信息，用于完整的设备更新。
 *
 * Copyright (c) 2007-2012
 * Qualcomm Technologies Incorporated.
 * All Rights Reserved.
 * Qualcomm Confidential and Proprietary
 *
 *****************************************************************************/
/*=============================================================================
                        Edit History

$Header: //source/qcom/qct/platform/uefi/workspaces/pweber/apps/8x26_emmcdl/emmcdl/main/latest/inc/ffu.h#7 $
$DateTime: 2015/05/07 21:41:17 $ $Author: pweber $

when       who     what, where, why
-------------------------------------------------------------------------------
11/04/12   pgw     Initial version.
=============================================================================*/
#pragma once
#include "partition.h"
#include "serialport.h"
#include "firehose.h"
#include "datatypes/bytearray.h"
#include <windows.h>

/**
 * @struct SECURITY_HEADER
 * @brief FFU security header structure.
 *        FFU 安全头结构体。
 *
 * This structure contains security information for the FFU image,
 * including signature, chunk size, algorithm ID, and hash table size.
 * 此结构体包含 FFU 镜像的安全信息，包括签名、块大小、算法 ID 和哈希表大小。
 */
typedef struct _SECURITY_HEADER
{
    uint32_t cbSize;           // Size of this header / 此头的大小
    BYTE   signature[12];      // Signature bytes / 签名字节
    uint32_t dwChunkSizeInKb;   // Chunk size in KB / 块大小（KB）
    uint32_t dwAlgId;           // Algorithm ID for encryption/signing / 加密/签名的算法 ID
    uint32_t dwCatalogSize;      // Catalog size / 目录大小
    uint32_t dwHashTableSize;    // Hash table size / 哈希表大小
} SECURITY_HEADER;

/**
 * @struct IMAGE_HEADER
 * @brief FFU image header structure.
 *        FFU 镜像头结构体。
 *
 * This structure contains the image header information including
 * signature and manifest length.
 * 此结构体包含镜像头信息，包括签名和清单长度。
 */
typedef struct _IMAGE_HEADER
{
    DWORD  cbSize;             // Size of this header / 此头的大小
    BYTE   Signature[12];       // Signature bytes / 签名字节
    DWORD  ManifestLength;      // Length of the manifest / 清单长度
    DWORD  dwChunkSize;        // Chunk size / 块大小
} IMAGE_HEADER;

/**
 * @struct STORE_HEADER
 * @brief FFU store header structure.
 *        FFU 存储头结构体。
 *
 * This structure contains the store header information including
 * platform ID, block size, and descriptor counts.
 * 此结构体包含存储头信息，包括平台 ID、块大小和描述符计数。
 */
typedef struct _STORE_HEADER
{
    uint32_t dwUpdateType;              // Update type / 更新类型
    uint16_t MajorVersion, MinorVersion; // Version numbers / 版本号
    uint16_t FullFlashMajorVersion, FullFlashMinorVersion; // Full flash version / 完整闪存版本
    char szPlatformId[192];             // Platform ID string / 平台 ID 字符串
    uint32_t dwBlockSizeInBytes;        // Block size in bytes / 块大小（字节）
    uint32_t dwWriteDescriptorCount;     // Number of write descriptors / 写入描述符数量
    uint32_t dwWriteDescriptorLength;   // Length of write descriptors / 写入描述符长度
    uint32_t dwValidateDescriptorCount;  // Number of validation descriptors / 验证描述符数量
    uint32_t dwValidateDescriptorLength;// Length of validation descriptors / 验证描述符长度
    uint32_t dwInitialTableIndex;       // Initial table index / 初始表索引
    uint32_t dwInitialTableCount;       // Initial table count / 初始表计数
    uint32_t dwFlashOnlyTableIndex;      // Flash-only table index / 仅闪存表索引
    uint32_t dwFlashOnlyTableCount;      // Flash-only table count / 仅闪存表计数
    uint32_t dwFinalTableIndex;          // Final table index / 最终表索引
    uint32_t dwFinalTableCount;          // Final table count / 最终表计数
} STORE_HEADER;

/**
 * @enum DISK_ACCESS_METHOD
 * @brief Disk access method enumeration.
 *        磁盘访问方法枚举。
 */
enum DISK_ACCESS_METHOD
{
    DISK_BEGIN  = 0,  // Access from beginning / 从开始访问
    DISK_SEQ    = 1,  // Sequential access / 顺序访问
    DISK_END    = 2   // Access from end / 从结束访问
};

/**
 * @struct DISK_LOCATION
 * @brief Disk location structure.
 *        磁盘位置结构体。
 *
 * This structure specifies the location on disk for data access.
 * 此结构体指定磁盘上的数据访问位置。
 */
typedef struct _DISK_LOCATION
{
    uint32_t dwDiskAccessMethod; // Disk access method / 磁盘访问方法
    uint32_t dwBlockIndex;        // Block index / 块索引
} DISK_LOCATION;

/**
 * @struct BLOCK_DATA_ENTRY
 * @brief Block data entry structure.
 *        块数据条目结构体。
 *
 * This structure contains block data information including
 * location count and disk locations.
 * 此结构体包含块数据信息，包括位置计数和磁盘位置。
 */
typedef struct _BLOCK_DATA_ENTRY
{
    uint32_t dwLocationCount;       // Number of disk locations / 磁盘位置数量
    uint32_t dwBlockCount;          // Number of blocks / 块数量
    DISK_LOCATION rgDiskLocations[1]; // Array of disk locations / 磁盘位置数组
} BLOCK_DATA_ENTRY;

/**
 * @class FFUImage
 * @brief FFU (Full Flash Update) image handler.
 *        FFU（完整固件更新）镜像处理器。
 *
 * This class provides functionality to parse and program FFU images
 * to devices using the Protocol interface. FFU images are used by
 * Windows Phone/Windows Mobile devices for complete system updates.
 * 本类提供解析和编程 FFU 镜像到设备的功能，使用 Protocol 接口。
 * FFU 镜像由 Windows Phone/Windows Mobile 设备用于完整的系统更新。
 */
class FFUImage {
public:
    /**
     * @brief Constructor.
     *        构造函数。
     */
    FFUImage();
    
    /**
     * @brief Destructor.
     *        析构函数。
     */
    ~FFUImage();
    
    /**
     * @brief Pre-load FFU image file.
     *        预加载 FFU 镜像文件。
     * @param szFFUPath [in] Path to the FFU file. FFU 文件路径。
     * @return Status code. 错误代码。
     */
    int PreLoadImage(const char *szFFUPath);
    
    /**
     * @brief Pre-load FFU image file (modern version).
     *        预加载 FFU 镜像文件（现代版本）。
     * @param szFFUPath [in] Path to the FFU file. FFU 文件路径。
     * @return Status code. 错误代码。
     */
    int PreLoadImage(const std::string& szFFUPath);
    
    /**
     * @brief Program FFU image to device.
     *        将 FFU 镜像编程到设备。
     * @param proto [in] Protocol pointer for device communication. 用于设备通信的协议指针。
     * @param dwOffset [in] Offset to write the image. 写入镜像的偏移量。
     * @return Status code. 错误代码。
     */
    int ProgramImage(Protocol *proto, int64_t dwOffset);
    
    /**
     * @brief Close the FFU file.
     *        关闭 FFU 文件。
     * @return Status code. 错误代码。
     */
    int CloseFFUFile(void);
    
    /**
     * @brief Set the disk sector size.
     *        设置磁盘扇区大小。
     * @param size [in] Sector size in bytes. 扇区大小（字节）。
     */
    void SetDiskSectorSize(int size);
    
    /**
     * @brief Split FFU binary file.
     *        分割 FFU 二进制文件。
     * @param szPartName [in] Partition name. 分区名称。
     * @param szOutputFile [in] Output file path. 输出文件路径。
     * @return Status code. 错误代码。
     */
    int SplitFFUBin(const char *szPartName, const char *szOutputFile);
    
    /**
     * @brief Split FFU binary file (modern version).
     *        分割 FFU 二进制文件（现代版本）。
     * @param szPartName [in] Partition name. 分区名称。
     * @param szOutputFile [in] Output file path. 输出文件路径。
     * @return Status code. 错误代码。
     */
    int SplitFFUBin(const std::string& szPartName, const std::string& szOutputFile);
    
    /**
     * @brief Convert FFU to raw program format.
     *        将 FFU 转换为原始程序格式。
     * @param szFFUName [in] FFU file name. FFU 文件名。
     * @param szImagePath [in] Output raw program file path. 输出原始程序文件路径。
     * @return Status code. 错误代码。
     */
    int FFUToRawProgram(const char *szFFUName, const char *szImagePath);
    
    /**
     * @brief Convert FFU to raw program format (modern version).
     *        将 FFU 转换为原始程序格式（现代版本）。
     * @param szFFUName [in] FFU file name. FFU 文件名。
     * @param szImagePath [in] Output raw program file path. 输出原始程序文件路径。
     * @return Status code. 错误代码。
     */
    int FFUToRawProgram(const std::string& szFFUName, const std::string& szImagePath);

private:
    /**
     * @brief Set the offset for overlapped I/O.
     *        设置重叠 I/O 的偏移量。
     * @param ovlVariable [in] Overlapped structure pointer. 重叠结构体指针。
     * @param offset [in] Offset value. 偏移量值。
     */
    void SetOffset(OVERLAPPED* ovlVariable, uint64_t offset);
    
    /**
     * @brief Create raw program file.
     *        创建原始程序文件。
     * @param szFFUFile [in] FFU file name. FFU 文件名。
     * @param szFileName [in] Output file name. 输出文件名。
     * @return Status code. 错误代码。
     */
    int CreateRawProgram(const char *szFFUFile, const char *szFileName);
    
    /**
     * @brief Create raw program file (modern version).
     *        创建原始程序文件（现代版本）。
     * @param szFFUFile [in] FFU file name. FFU 文件名。
     * @param szFileName [in] Output file name. 输出文件名。
     * @return Status code. 错误代码。
     */
    int CreateRawProgram(const std::string& szFFUFile, const std::string& szFileName);
    
    /**
     * @brief Terminate raw program file.
     *        终止原始程序文件。
     * @param szFileName [in] File name. 文件名。
     * @return Status code. 错误代码。
     */
    int TerminateRawProgram(const char *szFileName);
    
    /**
     * @brief Terminate raw program file (modern version).
     *        终止原始程序文件（现代版本）。
     * @param szFileName [in] File name. 文件名。
     * @return Status code. 错误代码。
     */
    int TerminateRawProgram(const std::string& szFileName);
    
    /**
     * @brief Dump raw program from FFU.
     *        从 FFU 转储原始程序。
     * @param szFFUFile [in] FFU file name. FFU 文件名。
     * @param szRawProgram [in] Raw program file name. 原始程序文件名。
     * @return Status code. 错误代码。
     */
    int DumpRawProgram(const char *szFFUFile, const char *szRawProgram);
    
    /**
     * @brief Dump raw program from FFU (modern version).
     *        从 FFU 转储原始程序（现代版本）。
     * @param szFFUFile [in] FFU file name. FFU 文件名。
     * @param szRawProgram [in] Raw program file name. 原始程序文件名。
     * @return Status code. 错误代码。
     */
    int DumpRawProgram(const std::string& szFFUFile, const std::string& szRawProgram);
    
    /**
     * @brief Dump FFU disk contents.
     *        转储 FFU 磁盘内容。
     * @param proto [in] Protocol pointer for device communication. 用于设备通信的协议指针。
     * @return Status code. 错误代码。
     */
    int FFUDumpDisk(Protocol *proto);
    
    /**
     * @brief Add entry to raw program file.
     *        向原始程序文件添加条目。
     * @param szRawProgram [in] Raw program file name. 原始程序文件名。
     * @param szFileName [in] File name. 文件名。
     * @param ui64FileOffset [in] File offset. 文件偏移量。
     * @param i64StartSector [in] Starting sector. 起始扇区。
     * @param ui64NumSectors [in] Number of sectors. 扇区数量。
     * @return Status code. 错误代码。
     */
    int AddEntryToRawProgram(const char *szRawProgram, const char *szFileName, uint64_t ui64FileOffset, int64_t i64StartSector, uint64_t ui64NumSectors);
    
    /**
     * @brief Add entry to raw program file (modern version).
     *        向原始程序文件添加条目（现代版本）。
     * @param szRawProgram [in] Raw program file name. 原始程序文件名。
     * @param szFileName [in] File name. 文件名。
     * @param ui64FileOffset [in] File offset. 文件偏移量。
     * @param i64StartSector [in] Starting sector. 起始扇区。
     * @param ui64NumSectors [in] Number of sectors. 扇区数量。
     * @return Status code. 错误代码。
     */
    int AddEntryToRawProgram(const std::string& szRawProgram, const std::string& szFileName, uint64_t ui64FileOffset, int64_t i64StartSector, uint64_t ui64NumSectors);
    
    /**
     * @brief Read GPT (GUID Partition Table).
     *        读取 GPT（GUID 分区表）。
     * @return Status code. 错误代码。
     */
    int ReadGPT(void);
    
    /**
     * @brief Parse FFU headers.
     *        解析 FFU 头部。
     * @return Status code. 错误代码。
     */
    int ParseHeaders(void);
    
    /**
     * @brief Get the next starting area.
     *        获取下一个起始区域。
     * @param chunkSizeInBytes [in] Chunk size in bytes. 块大小（字节）。
     * @param sizeOfArea [in] Size of the area. 区域大小。
     * @return Next starting area offset. 下一个起始区域偏移量。
     */
    uint64_t GetNextStartingArea(uint64_t chunkSizeInBytes, uint64_t sizeOfArea);

    SECURITY_HEADER FFUSecurityHeader;  // FFU security header / FFU 安全头
    IMAGE_HEADER FFUImageHeader;       // FFU image header / FFU 镜像头
    STORE_HEADER FFUStoreHeader;       // FFU store header / FFU 存储头
    BYTE* ValidationEntries;            // Validation entries / 验证条目
    BLOCK_DATA_ENTRY* BlockDataEntries; // Block data entries / 块数据条目
    uint64_t PayloadDataStart;         // Start of payload data / 载荷数据起始位置

    HANDLE hFFU;                      // Handle to the FFU file / FFU 文件句柄
    OVERLAPPED OvlRead;               // Overlapped structure for reading / 读取用的重叠结构体
    OVERLAPPED OvlWrite;              // Overlapped structure for writing / 写入用的重叠结构体

    BYTE GptProtectiveMBR[512];       // GPT protective MBR / GPT 保护性 MBR
    gpt_header_t GptHeader;           // GPT header / GPT 头
    gpt_entry_t *GptEntries;          // GPT entries / GPT 条目
    int DISK_SECTOR_SIZE;             // Disk sector size / 磁盘扇区大小
};
