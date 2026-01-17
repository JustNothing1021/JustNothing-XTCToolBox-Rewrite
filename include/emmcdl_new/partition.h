/*****************************************************************************
 * partition.h
 *
 * This file implements partition table handling and XML parsing for device flashing
 * 本文件实现了设备刷机的分区表处理和 XML 解析
 *
 * This module provides functionality to parse partition tables (GPT),
 * process XML configuration files for flashing operations, and manage
 * partition entries with various commands (program, erase, patch, etc.).
 * 此模块提供解析分区表（GPT）、处理刷机操作的 XML 配置文件以及
 * 管理各种命令（编程、擦除、修补等）的分区条目的功能。
 *
 *****************************************************************************/

#pragma once

#include <windows.h>
#include <stdint.h>
#include <tchar.h>
#include <string>
#include <winerror.h>
#include <stdlib.h>
#include <stdio.h>
#include <emmcdl_new/xmlparser.h>
#include <datatypes/bytearray.h>

#define MAX_LIST_SIZE 100
#define MAX_PATH_LEN 256
#define SECTOR_SIZE 512

class Protocol;

/**
 * @enum cmdEnum
 * @brief Partition command enumeration.
 *        分区命令枚举。
 *
 * This enumeration defines the types of commands that can be executed
 * on partitions during flashing operations.
 * 此枚举定义了在刷机操作中可以对分区执行的命令类型。
 */
enum cmdEnum {
    CMD_INVALID = 0,  // Invalid command / 无效命令
    CMD_PATCH = 1,     // Patch command / 修补命令
    CMD_ZEROOUT = 2,   // Zero out command / 清零命令
    CMD_PROGRAM = 3,    // Program command / 编程命令
    CMD_OPTION = 4,    // Option command / 选项命令
    CMD_PATH = 5,       // Path command / 路径命令
    CMD_READ = 6,       // Read command / 读取命令
    CMD_ERASE = 7,      // Erase command / 擦除命令
    CMD_NOP = 8         // No operation / 空操作
};

/**
 * @struct gpt_header_t
 * @brief GPT (GUID Partition Table) header structure.
 *        GPT（GUID 分区表）头结构体。
 *
 * This structure contains the header information for a GPT partition table.
 * GPT is a standard for the layout of the partition table on a physical
 * hard disk, using globally unique identifiers (GUIDs).
 * 此结构体包含 GPT 分区表的头部信息。GPT 是物理硬盘上分区表
 * 布局的标准，使用全局唯一标识符（GUID）。
 */
typedef struct {
    char signature[8];    // GPT signature ("EFI PART") / GPT 签名（"EFI PART"）
    int32_t revision;     // GPT revision number / GPT 修订号
    int32_t header_size;  // Size of header in bytes / 头大小（字节）
    int32_t crc_header;   // CRC32 of header / 头的 CRC32 校验和
    int32_t reserved;      // Reserved / 保留字段
    uint64_t current_lba; // LBA of this header copy / 此头副本的 LBA
    uint64_t backup_lba;   // LBA of backup header / 备份头的 LBA
    uint64_t first_lba;    // LBA of first usable partition / 第一个可用分区的 LBA
    uint64_t last_lba;     // LBA of last usable partition / 最后一个可用分区的 LBA
    char disk_guid[16];    // GUID of the disk / 磁盘的 GUID
    uint64_t partition_lba; // LBA of partition entries / 分区条目的 LBA
    int32_t num_entries;   // Number of partition entries / 分区条目数量
    int32_t entry_size;    // Size of partition entry / 分区条目大小
    int32_t crc_partition; // CRC32 of partition entries / 分区条目的 CRC32 校验和
    char reserved2[420];   // Reserved / 保留字段
} gpt_header_t;

/**
 * @struct gpt_entry_t
 * @brief GPT partition entry structure.
 *        GPT 分区条目结构体。
 *
 * This structure contains the information for a single partition entry
 * in a GPT partition table.
 * 此结构体包含 GPT 分区表中单个分区条目的信息。
 */
typedef struct {
    char       type_guid[16];  // Partition type GUID / 分区类型 GUID
    char       unique_guid[16]; // Unique partition GUID / 唯一分区 GUID
    uint64_t   first_lba;     // First LBA of partition / 分区的起始 LBA
    uint64_t   last_lba;      // Last LBA of partition / 分区的结束 LBA
    uint64_t   attributes;     // Partition attributes / 分区属性
    char       part_name[36];  // Partition name (UTF-16) / 分区名称（UTF-16）
} gpt_entry_t;

/**
 * @struct PartitionEntry
 * @brief Partition entry structure for XML parsing.
 *        XML 解析用的分区条目结构体。
 *
 * This structure contains the information for a partition entry
 * parsed from an XML configuration file.
 * 此结构体包含从 XML 配置文件解析的分区条目信息。
 */
typedef struct {
    cmdEnum     eCmd;                         // Command type / 命令类型
    uint64_t    start_sector;                  // Starting sector / 起始扇区
    uint64_t    offset;                        // Offset / 偏移量
    uint64_t    num_sectors;                   // Number of sectors / 扇区数量
    uint8_t     physical_partition_number;      // Physical partition number / 物理分区号
    uint64_t    patch_value;                   // Patch value / 修补值
    uint64_t    patch_offset;                  // Patch offset / 修补偏移量
    uint64_t    patch_size;                    // Patch size / 修补大小
    uint64_t    crc_start;                     // CRC start / CRC 起始位置
    uint64_t    crc_size;                      // CRC size / CRC 大小
    std::string filename;                     // Filename / 文件名
} PartitionEntry;

/**
 * @brief Replace a substring in a C string.
 *        替换 C 字符串中的子字符串。
 * @param inp [in] Input string. 输入字符串。
 * @param find [in] String to find. 要查找的字符串。
 * @param rep [in] Replacement string. 替换字符串。
 * @return Modified string. 修改后的字符串。
 */
char* StringReplace(char* inp, const char* find, const char* rep);

/**
 * @brief Set a value in a key-value pair string.
 *        在键值对字符串中设置值。
 * @param key [in] Key string. 键字符串。
 * @param keyName [in] Key name. 键名。
 * @param value [in] Value to set. 要设置的值。
 * @return Modified string. 修改后的字符串。
 */
char* StringSetValue(char* key, char* keyName, char* value);

/**
 * @brief Replace a substring in a std::string.
 *        替换 std::string 中的子字符串。
 * @param inp [in] Input string. 输入字符串。
 * @param find [in] String to find. 要查找的字符串。
 * @param rep [in] Replacement string. 替换字符串。
 * @return Modified string. 修改后的字符串。
 */
std::string StringReplace(std::string inp, const std::string find, const std::string rep);

/**
 * @brief Set a value in a key-value pair std::string.
 *        在键值对 std::string 中设置值。
 * @param key [in] Key string. 键字符串。
 * @param keyName [in] Key name. 键名。
 * @param value [in] Value to set. 要设置的值。
 * @return Modified string. 修改后的字符串。
 */
std::string StringSetValue(std::string key, std::string keyName, std::string value);

/**
 * @class Partition
 * @brief Partition table handler and XML parser.
 *        分区表处理器和 XML 解析器。
 *
 * This class provides functionality to parse partition tables (GPT),
 * process XML configuration files for flashing operations, and manage
 * partition entries with various commands.
 * 本类提供解析分区表（GPT）、处理刷机操作的 XML 配置文件以及
 * 管理各种命令的分区条目的功能。
 */
class Partition : public XMLParser {
public:
    int num_entries;     // Number of partition entries / 分区条目数量
    int cur_action;      // Current action index / 当前操作索引
    uint64_t d_sectors; // Disk sectors / 磁盘扇区数

    /**
     * @brief Constructor.
     *        构造函数。
     * @param ds [in] Disk sectors (optional). 磁盘扇区数（可选）。
     */
    Partition(uint64_t ds = 0) {
        num_entries = 0;
        cur_action = 0;
        d_sectors = ds;
    };
    
    /**
     * @brief Destructor.
     *        析构函数。
     */
    ~Partition() { CloseXML(); };
    
    /**
     * @brief Close the XML file.
     *        关闭 XML 文件。
     * @return Status code. 错误代码。
     */
    int CloseXML();
    
    /**
     * @brief Pre-load the XML file.
     *        预加载 XML 文件。
     * @param fname [in] Path to the XML file. XML 文件路径。
     * @return Status code. 错误代码。
     */
    int PreLoadImage(const std::string& fname);
    
    /**
     * @brief Program partitions according to XML file.
     *        根据 XML 文件操作分区。
     * @param proto [in] Protocol object. 协议对象。
     * @return Status code. 错误代码。
     */
    int ProgramImage(Protocol* proto);
    
    /**
     * @brief Process a single PartitionEntry.
     *        处理单个 PartitionEntry。
     * @param proto [in] Protocol object. 协议对象。
     * @param pe [in] Partition operation parameters. 分区操作参数。
     * @param key [in] Key from XML file. XML 文件中的键。
     * @return Status code. 错误代码。
     */
    int ProgramPartitionEntry(Protocol* proto, PartitionEntry pe, const std::string& key);
    
    /**
     * @brief Parse XML expression and evaluate its value.
     *        解析 XML 表达式并计算其值。
     * @param expr [in] XML expression to parse. 要解析的 XML 表达式。
     * @param value [out] Evaluated value. 解析后的值。
     * @param pe [in] Partition operation parameters. 分区操作参数。
     * @return Status code. 错误代码。
     */
    int ParseXMLEvaluate(std::string expr, uint64_t& value, PartitionEntry* pe);
    
    /**
     * @brief Parse XML key and populate PartitionEntry structure.
     *        解析 XML 键并根据数据填充 PartitionEntry 结构体。
     * @param key [in] XML key to parse. 要解析的 XML 键。
     * @param pe [out] Populated PartitionEntry structure. 填充的 PartitionEntry 结构体。
     * @return Status code. 错误代码。
     */
    int ParseXMLKey(TCHAR* key, PartitionEntry* pe);
    
    /**
     * @brief Parse XML key and populate PartitionEntry structure.
     *        解析 XML 键并根据数据填充 PartitionEntry 结构体。
     * @param line [in] XML line to parse. 要解析的 XML 行。
     * @param pe [out] Populated PartitionEntry structure. 填充的 PartitionEntry 结构体。
     * @return Status code. 错误代码。
     */
    int ParseXMLKey(const std::string& line, PartitionEntry* pe);
    
    /**
     * @brief Get the next XML key.
     *        获取下一个 XML 键。
     * @param keyName [out] Key name. 键名。
     * @param key [out] Key value. 键值。
     * @return Status code. 错误代码。
     */
    int GetNextXMLKey(std::string& keyName, std::string& key);
    
    /**
     * @brief Calculate CRC32 checksum for given buffer.
     *        计算给定缓冲区的 CRC32 校验和。
     * @param buffer [in] Buffer to calculate checksum. 要计算校验和的缓冲区。
     * @param len [in] Length of buffer. 缓冲区的长度。
     * @return Calculated CRC32 checksum. 计算得到的 CRC32 校验和。
     */
    unsigned int CalcCRC32(const BYTE* buffer, int len);
    
    /**
     * @brief Calculate CRC32 checksum for given buffer (modern version).
     *        计算给定缓冲区的 CRC32 校验和（现代版本）。
     * @param buffer [in] Buffer to calculate checksum. 要计算校验和的缓冲区。
     * @param len [in] Length of buffer. 缓冲区的长度。
     * @return Calculated CRC32 checksum. 计算得到的 CRC32 校验和。
     */
    unsigned int CalcCRC32(const ByteArray& buffer, int len);
    
    /**
     * @brief Parse an integer value from an XML line.
     *        解析一个 XML 行中的整数值。
     * @param line [in] XML line to parse. 要解析的 XML 行。
     * @param key [in] Key to find. 要查找的键。
     * @param value [out] Parsed integer value. 解析得到的整数值。
     * @param pe [in] Partition operation parameters. 分区操作参数。
     * @return Status code. 错误代码。
     */
    int ParseXMLInt64(const std::string& line, const std::string& key, uint64_t& value, PartitionEntry* pe);

private:
    char* xmlStart;  // Start of XML data / XML 数据起始位置
    char* xmlEnd;    // End of XML data / XML 数据结束位置
    char* keyStart;  // Start of key data / 键数据起始位置
    char* keyEnd;    // End of key data / 键数据结束位置

    /**
     * @brief Reflect the bit order of data.
     *        反转数据的位顺序。
     * @param data [in] Data to reflect. 要反转的数据。
     * @param len [in] Bit length of data. 数据的位长度。
     * @return Reflected data. 反转后的数据。
     */
    int Reflect(int data, int len);
    
    /**
     * @brief Parse Options field in XML file (not implemented).
     *        解析 XML 文件中的 Options 字段（未实现）。
     * @return Status code. 错误代码。
     */
    int ParseXMLOptions();
    
    /**
     * @brief Parse PathList field in XML file (not implemented).
     *        解析 XML 文件中的 PathList 字段（未实现）。
     * @return Status code. 错误代码。
     */
    int ParsePathList();
    
    /**
     * @brief Check if a string is an empty line.
     *        检查字符串是否为空行。
     * @param str [in] String to check. 要检查的字符串。
     * @return True if string is empty line, false otherwise. 如果字符串为空行则返回 true，否则返回 false。
     */
    bool CheckEmptyLine(std::string str);
};
