/*****************************************************************************
* protocol.h
*
* This file implements the streaming download protocol
*
* Copyright (c) 2007-2015
* Qualcomm Technologies Incorporated.
* All Rights Reserved.
* Qualcomm Confidential and Proprietary
*
*****************************************************************************/
/*=============================================================================
Edit History

$Header: //source/qcom/qct/platform/uefi/workspaces/pweber/apps/8x26_emmcdl/emmcdl/main/latest/inc/protocol.h#4 $
$DateTime: 2015/05/07 21:41:17 $ $Author: pweber $

when       who     what, where, why
-------------------------------------------------------------------------------
12/09/15   pgw     Initial version.
=============================================================================*/
#pragma once

#include "partition.h"
#include <windows.h>

#define MAX_XML_LEN         2048
#define MAX_TRANSFER_SIZE   0x100000

class Protocol {
public:

    Protocol();
    ~Protocol();

    int DumpDiskContents(uint64_t start_sector, uint64_t num_sectors, TCHAR* szOutFile, uint8_t partNum, TCHAR* szPartName);
    int WipeDiskContents(uint64_t start_sector, uint64_t num_sectors, TCHAR* szPartName);

    int ReadGPT(bool debug);
    int WriteGPT(TCHAR* szPartName, TCHAR* szBinFile);
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
    void Log(const char* str, ...);
    void Log(const TCHAR* str, ...);

    gpt_header_t gpt_header;
    gpt_entry_t* gpt_entries;
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
