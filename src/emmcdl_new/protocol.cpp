
#include "emmcdl_new/protocol.h"
#include "emmcdl_new/utils.h"
#include "utils/logger.h"
#include "utils/string_utils.h"

Protocol::Protocol(void) {
    hDisk = INVALID_HANDLE_VALUE;
    buffer1 = buffer2 = nullptr;
    gpt_entries = nullptr;
    DISK_SECTOR_SIZE = 512;
    
    // 分配对齐缓冲区
    bufAlloc1 = (BYTE*) malloc(MAX_TRANSFER_SIZE + 0x200);
    bufAlloc2 = (BYTE*) malloc(MAX_TRANSFER_SIZE + 0x200);
    
    if (!bufAlloc1 || !bufAlloc2) {
        LERROR("Protocol::Protocol", "内存分配失败");
        throw std::bad_alloc();
    }
    
    if (bufAlloc1) buffer1 = (BYTE*) (((uint64_t) bufAlloc1 + 0x200) & ~0x1ff);
    if (bufAlloc2) buffer2 = (BYTE*) (((uint64_t) bufAlloc2 + 0x200) & ~0x1ff);
    
    LDEBUG("Protocol::Protocol", "协议对象初始化完成");
}

Protocol::~Protocol(void) {
    // 释放所有分配的资源
    if (bufAlloc1) {
        free(bufAlloc1);
        bufAlloc1 = nullptr;
    }
    if (bufAlloc2) {
        free(bufAlloc2);
        bufAlloc2 = nullptr;
    }
    if (gpt_entries) {
        free(gpt_entries);
        gpt_entries = nullptr;
    }
    
    LDEBUG("Protocol::~Protocol", "协议对象资源已释放");
}

void Protocol::EnableVerbose(void) {
    bVerbose = true;
}


int Protocol::LoadPartitionInfo(std::string szPartName, PartitionEntry* pEntry) {
    int status = ERROR_SUCCESS;
    if (gpt_entries == nullptr) {
        LDEBUG("Protocol::LoadPartitionInfo", "分区信息为空，尝试读取分区表");
        status = ReadGPT(false);
        if (status != ERROR_SUCCESS) {
            LWARN("Protocol::LoadPartitionInfo", "获取分区信息时读取分区表失败");
            return status;
        }
    }
    status = ERROR_NOT_FOUND;
    memset(pEntry, 0, sizeof(PartitionEntry));
    for (int i = 0; i < 128; i++) {
        if (strcmp(szPartName.c_str(), gpt_entries[i].part_name) == 0) {
            pEntry->start_sector = gpt_entries[i].first_lba;
            pEntry->num_sectors = gpt_entries[i].last_lba - gpt_entries[i].first_lba + 1;
            pEntry->physical_partition_number = 0;
            return ERROR_SUCCESS;
        }
    }
    if (status != ERROR_SUCCESS) {
        LERROR("Protocol::LoadPartitionInfo", "未找到分区 \"%s\"", szPartName.c_str());
    }
    return status;
}

int Protocol::WriteGPT(std::string szPartName, std::string szBinFile) {
    int status = ERROR_SUCCESS;
    PartitionEntry partEntry;

    if (LoadPartitionInfo(szPartName, &partEntry) == ERROR_SUCCESS) {
        Partition partition;
        partEntry.filename = szBinFile;
        partEntry.eCmd = CMD_PROGRAM;
        string cmd_pkt = fmt::format(
            "<program SECTOR_SIZE_IN_BYTES=\"{}\" num_partition_sectors=\"{}\" physical_partition_number=\"0\" start_sector=\"{}\"/>",
            DISK_SECTOR_SIZE,
            (int) partEntry.num_sectors, (int) partEntry.start_sector
        );
        status = partition.ProgramPartitionEntry(this, partEntry, cmd_pkt);
    }

    return status;
}

int Protocol::ReadGPT(bool show_result) {
    int status = ERROR_SUCCESS;
    DWORD bytesRead;
    gpt_header_t gpt_header;
    
    // 分配内存存储分区表条目
    gpt_entries = (gpt_entry_t*) malloc(sizeof(gpt_entry_t) * 128);

    if (gpt_entries == nullptr) {
        LERROR("Protocol::ReadGPT", "存储结果时内存分配失败");
        return ERROR_OUTOFMEMORY;
    }

    // 从第一个扇区读取GPT头
    LDEBUG("Protocol::ReadGPT", "读取分区头");

    status = ReadData(
        (BYTE*) &gpt_header,
        DISK_SECTOR_SIZE,
        DISK_SECTOR_SIZE,
        &bytesRead,
        0
    );

    if ((status == ERROR_SUCCESS) && (memcmp("EFI PART", gpt_header.signature, 8) == 0)) {
        LDEBUG("Protocol::ReadGPT", "成功找到分区表，开始读取分区表信息");
        status = ReadData(
            (BYTE*) gpt_entries,
            2 * DISK_SECTOR_SIZE,
            32 * DISK_SECTOR_SIZE,
            &bytesRead,
            0
        );
        if ((status == ERROR_SUCCESS) && show_result)
            LINFO("Protocol::ReadGPT", "分区信息: ");
            for (int i = 0; (i < gpt_header.num_entries) && (i < 128); i++)
                if (gpt_entries[i].first_lba > 0)
                    LINFO(
                        "Protocol::ReadGPT",
                        "[%2d] 分区名 %16s    起始扇区 %12llu  大小 %12llu",
                        i + 1,
                        gpt_entries[i].part_name,
                        gpt_entries[i].first_lba, gpt_entries[i].last_lba - gpt_entries[i].first_lba + 1
                    );
    } else {
        LWARN("Protocol::ReadGPT", "分区头无效, 获取到的签名: \"%s\"（预期为\"EFI PART\"），返回ERROR_INVALID_DATA", gpt_header.signature);
        free(gpt_entries);
        gpt_entries = NULL;
        status = ERROR_INVALID_DATA;
    }
    return status;
}

uint64_t Protocol::GetNumDiskSectors() {
    return disk_size / DISK_SECTOR_SIZE;
}

void Protocol::SetDiskSectorSize(int size) {
    DISK_SECTOR_SIZE = size;
}

int Protocol::GetDiskSectorSize(void) {
    return DISK_SECTOR_SIZE;
}

HANDLE Protocol::GetDiskHandle(void) {
    return hDisk;
}


int Protocol::DumpDiskContents(uint64_t start_sector, uint64_t num_sectors, std::string szOutFile, uint8_t partNum, const std::string& szPartName) {
    int status = ERROR_SUCCESS;
    
    // If there is a partition name provided load the info for the partition name
    if (!szPartName.empty()) {
        PartitionEntry pe;
        if (LoadPartitionInfo(szPartName, &pe) == ERROR_SUCCESS) {
            start_sector = pe.start_sector;
            num_sectors = pe.num_sectors;
        } else {
            return ERROR_FILE_NOT_FOUND;
        }
    }
    
    HANDLE hOutFile = CreateFileA(szOutFile.c_str(),
        GENERIC_WRITE | GENERIC_READ,
        0, // We want exclusive access to this disk
        NULL,
        CREATE_ALWAYS,
        0,
        NULL);
    if (hOutFile == INVALID_HANDLE_VALUE) {
        status = GetLastError();
        LWARN("Protocol::DumpDiskContents", "创建文件句柄失败，状态：%s", 
            getErrorDescription(status).c_str());
        return status;
    }
    status = FastCopy(hDisk, start_sector, hOutFile, 0, num_sectors, partNum);
    CloseHandle(hOutFile);
    if (status != ERROR_SUCCESS) {
        LWARN("Protocol::DumpDiskContents", "复制数据失败，状态：%s", 
            getErrorDescription(status).c_str());
    }
    return status;
}

int Protocol::DumpDiskContents(const std::string& szPartName, const std::string& szOutFile, uint8_t partNum) {
    int status = ERROR_SUCCESS;
    int start_sector = 0;
    int num_sectors = 0;
    PartitionEntry pe;
    if (status = LoadPartitionInfo(szPartName, &pe) == ERROR_SUCCESS) {
        start_sector = pe.start_sector;
        num_sectors = pe.num_sectors;
    } else {
        LWARN("Protocol::DumpDiskContents", "尝试获取分区扇区数时加载分区信息失败，状态：%s", 
            getErrorDescription(status).c_str());
        return status;
    }
    HANDLE hOutFile = CreateFileA(szOutFile.c_str(),
        GENERIC_WRITE | GENERIC_READ,
        0,
        NULL,
        CREATE_ALWAYS,
        0,
        NULL);

    if (hOutFile == INVALID_HANDLE_VALUE) {
        status = GetLastError();
        LWARN("Protocol::DumpDiskContents", "创建文件句柄失败，状态：%s", 
            getErrorDescription(status).c_str());
        return status;
    }
    status = FastCopy(hDisk, start_sector, hOutFile, 0, num_sectors, partNum);
    if (status != ERROR_SUCCESS) {
        LWARN("Protocol::DumpDiskContents", "复制数据失败，状态：%s", 
            getErrorDescription(status).c_str()
        );
    }
    CloseHandle(hOutFile);

    return status;
}

int Protocol::WipeDiskContents(uint64_t start_sector, uint64_t num_sectors, const std::string& szPartName) {
    PartitionEntry pe;
    int status = ERROR_SUCCESS;
    
    // If there is a partition name provided load the info for the partition name
    if (!szPartName.empty()) {
        if (LoadPartitionInfo(szPartName, &pe) == ERROR_SUCCESS) {
            start_sector = pe.start_sector;
            num_sectors = pe.num_sectors;
        } else {
            return ERROR_FILE_NOT_FOUND;
        }
    }
    
    // struct还是太强大了，string这种类型在memset之后竟然还能用
    memset(&pe, 0, sizeof(pe));
    pe.start_sector = start_sector;
    pe.filename = "ZERO";       // ZERO代表直接擦除扇区
    pe.num_sectors = num_sectors;
    pe.eCmd = CMD_ERASE;
    pe.physical_partition_number = 0; // 一般来说擦除只会在分区0上进行
    // 原文：By default the wipe disk only works on physical sector 0
    string cmd_pkt = fmt::format(
        "<program SECTOR_SIZE_IN_BYTES=\"{}\" num_partition_sectors=\"{}\" physical_partition_number=\"0\" start_sector=\"{}\"/>",
        DISK_SECTOR_SIZE,
        (int) num_sectors, (int) start_sector
    );
    Partition partition;
    status = partition.ProgramPartitionEntry(this, pe, cmd_pkt);
    return status;
}


int Protocol::WipeDiskContents(const std::string& szPartName) {
    PartitionEntry pe;
    int status = ERROR_SUCCESS;
    int start_sector = 0;
    int num_sectors = 0;

    if (LoadPartitionInfo(szPartName, &pe) == ERROR_SUCCESS) {
        start_sector = pe.start_sector;
        num_sectors = pe.num_sectors;
    } else {
        LWARN("Protocol::WipeDiskContents", "尝试获取分区扇区数时加载分区信息失败，状态：%s", 
            getErrorDescription(status).c_str());
        return status;
    }

    memset(&pe, 0, sizeof(pe));
    pe.filename = "ZERO";
    pe.start_sector = start_sector;
    pe.num_sectors = num_sectors;
    pe.eCmd = CMD_ERASE;
    pe.physical_partition_number = 0;

    string cmd_pkt = fmt::format(
        "<program SECTOR_SIZE_IN_BYTES=\"{}\" num_partition_sectors=\"{}\" physical_partition_number=\"0\" start_sector=\"{}\"/>",
        DISK_SECTOR_SIZE,
        (int) num_sectors, (int) start_sector
    );
    Partition partition;
    status = partition.ProgramPartitionEntry(this, pe, cmd_pkt);

    return status;
}

