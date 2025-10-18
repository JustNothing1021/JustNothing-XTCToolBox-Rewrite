#pragma once

#include <windows.h>
#include <stdint.h>
#include <tchar.h>
#include <string>
#include <winerror.h>
#include <stdlib.h>
#include <stdio.h>
#include <emmcdl_new/xmlparser.h>

#define MAX_LIST_SIZE 100
#define MAX_PATH_LEN 256
#define SECTOR_SIZE 512

class Protocol;

enum cmdEnum {
    CMD_INVALID = 0,
    CMD_PATCH = 1,
    CMD_ZEROOUT = 2,
    CMD_PROGRAM = 3,
    CMD_OPTION = 4,
    CMD_PATH = 5,
    CMD_READ = 6,
    CMD_ERASE = 7,
    CMD_NOP = 8
};

typedef struct {
    char signature[8];
    int32_t revision;
    int32_t header_size;
    int32_t crc_header;
    int32_t reserved;
    uint64_t current_lba;
    uint64_t backup_lba;
    uint64_t first_lba;
    uint64_t last_lba;
    char disk_guid[16];
    uint64_t partition_lba;
    int32_t num_entries;
    int32_t entry_size;
    int32_t crc_partition;
    char reserved2[420];
} gpt_header_t;

typedef struct {
    char type_guid[16];
    char unique_guid[16];
    uint64_t first_lba;
    uint64_t last_lba;
    uint64_t attributes;
    TCHAR part_name[36];
} gpt_entry_t;

typedef struct {
    cmdEnum eCmd;
    uint64_t start_sector;
    uint64_t offset;
    uint64_t num_sectors;
    uint8_t physical_partition_number;
    uint64_t patch_value;
    uint64_t patch_offset;
    uint64_t patch_size;
    uint64_t crc_start;
    uint64_t crc_size;
    TCHAR filename[MAX_PATH];
} PartitionEntry;

TCHAR* StringReplace(TCHAR* inp, const TCHAR* find, const TCHAR* rep);
TCHAR* StringSetValue(TCHAR* key, TCHAR* keyName, TCHAR* value);

std::string StringReplace(std::string inp, const std::string find, const std::string rep);
std::string StringSetValue(std::string key, std::string keyName, std::string value);

class Partition {
public:
    int num_entries;

    Partition(uint64_t ds = 0) {
        num_entries = 0;
        cur_action = 0;
        d_sectors = ds;
    };
    ~Partition() {};
    int PreLoadImage(TCHAR* fname);
    int ProgramImage(Protocol* proto);
    int ProgramPartitionEntry(Protocol* proto, PartitionEntry pe, TCHAR* key);

    int CloseXML();
    int GetNextXMLKey(TCHAR* keyName, TCHAR** key);
    unsigned int CalcCRC32(BYTE* buffer, int len);
    int ParseXMLKey(TCHAR* key, PartitionEntry* pe);

private:
    TCHAR* xmlStart;
    TCHAR* xmlEnd;
    TCHAR* keyStart;
    TCHAR* keyEnd;

    int cur_action;
    uint64_t d_sectors;

    int Reflect(int data, int len);
    int ParseXMLOptions();
    int ParsePathList();
    int ParseXMLString(const TCHAR* line, const TCHAR* key, TCHAR* value);
    int ParseXMLInt64(TCHAR* line, const TCHAR* key, uint64_t& value, PartitionEntry* pe);
    int ParseXMLEvaluate(TCHAR* expr, uint64_t& value, PartitionEntry* pe);
    bool CheckEmptyLine(TCHAR* str);
    int Log(const char* str, ...);
    int Log(const TCHAR* str, ...);

    HANDLE hLog;
};
