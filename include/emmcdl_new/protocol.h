
#pragma once

#include "emmcdl_new/partition.h"
#include <string>
#include <vector>
#include <windows.h>

#define MAX_XML_LEN         2048
#define MAX_TRANSFER_SIZE   0x100000

class Protocol {
public:

    Protocol();
    ~Protocol();

    int DumpDiskContents(uint64_t start_sector, uint64_t num_sectors, TCHAR* szOutFile, uint8_t partNum, TCHAR* szPartName);
    int DumpDiskContents(uint64_t start_sector, uint64_t num_sectors, std::string szOutFile, uint8_t partNum);
    int DumpDiskContents(std::string szPartName, std::string szOutFile, uint8_t partNum);
    int WipeDiskContents(uint64_t start_sector, uint64_t num_sectors, TCHAR* szPartName);
    int WipeDiskContents(uint64_t start_sector, uint64_t num_sectors, std::string szPartName);
    int WipeDiskContents(std::string szPartName);

    int ReadGPT(bool debug);
    int ReadGPT(bool debug, std::vector<gpt_entry_t>& partitions);
    int WriteGPT(TCHAR* szPartName, TCHAR* szBinFile);
    int WriteGPT(std::string szPartName, std::string szBinFile);
    void EnableVerbose(void);
    int GetDiskSectorSize(void);
    void SetDiskSectorSize(int size);
    uint64_t GetNumDiskSectors(void);
    HANDLE GetDiskHandle(void);

    virtual int DeviceReset(void) = 0;
    virtual int WriteData(BYTE* writeBuffer, int64_t writeOffset, DWORD writeBytes, DWORD* bytesWritten, uint8_t partNum) = 0;
    virtual int ReadData(BYTE* readBuffer, int64_t readOffset, DWORD readBytes, DWORD* bytesRead, uint8_t partNum) = 0;
    virtual int FastCopy(HANDLE hRead, int64_t sectorRead, HANDLE hWrite, int64_t sectorWrite, uint64_t sectors, uint8_t partNum) = 0;
    virtual int ProgramRawCommand(TCHAR* key) = 0;
    virtual int ProgramPatchEntry(PartitionEntry pe, TCHAR* key) = 0;

protected:

    int LoadPartitionInfo(TCHAR* szPartName, PartitionEntry* pEntry);
    int LoadPartitionInfo(std::string szPartName, PartitionEntry* pEntry);
    void Log(const char* str, ...);
    void Log(const TCHAR* str, ...);

    gpt_header_t gpt_hdr;
    gpt_entry_t* gpt_entries;
    std::vector<gpt_entry_t> v_gpt_entries; 
    uint64_t disk_size;
    HANDLE hDisk;
    BYTE* buffer1;
    BYTE* buffer2;
    BYTE* bufAlloc1;
    BYTE* bufAlloc2;
    int DISK_SECTOR_SIZE;

private:
    bool bVerbose;

};
