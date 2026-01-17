#include "emmcdl_new/dload.h"
#include "emmcdl_new/partition.h"
#include "emmcdl_new/diskwriter.h"
#include "emmcdl_new/utils.h"
#include "utils/logger.h"


void Dload::HexTobyte(const char* hex, BYTE* bin, int len) {
    // 将十六进制字符串转换为字节数组
    for (int i = 0; i < len; i++) {
        BYTE val1 = hex[i * 2] - '0';
        BYTE val2 = hex[i * 2 + 1] - '0';
        if (val1 > 9) val1 = val1 - 7;
        if (val2 > 9) val2 = val2 - 7;
        bin[i] = (val1 << 4) + val2;
    }
}

Dload::Dload(SerialPort* port) {
    // 初始化串口
    bSectorAddress = false;
    sport = port;
    
    if (!sport) {
        LERROR("Dload::Dload", "串口指针为空");
        throw std::invalid_argument("串口指针不能为空");
    }
}

int Dload::ConnectToFlashProg(BYTE ver) {

    ByteArray bHello;
    bHello += EHOST_HELLO_REQ;
    bHello += ByteArray("QCOM fast download protocol host");
    bHello += ver;
    bHello += 2;
    bHello += 1;

    ByteArray bSecurity({EHOST_SECURITY_REQ, 0});
    ByteArray bRsp;
    int status = ERROR_SUCCESS;
    int i = 0;

    if (ver > 2) {
        bHello[35] |= FEATURE_SECTOR_ADDRESSES;
        bSectorAddress = true;
    }

    // Hello和Security包各发送10次，每次间隔500ms
    // 设备重新连接时初始连接可能需要一段时间且不稳定
    for (; i < 10; i++) {
        status = sport->SendSync(bHello, bRsp);
        if ((status == ERROR_SUCCESS) && (bRsp.size() > 0) && (bRsp[0] == EHOST_HELLO_RSP)) {
            LDEBUG("Dload::ConnectToFlashProg", "已接收到Hello响应");
            break;
        }
        sport->Flush();
        LDEBUG("Dload::ConnectToFlashProg", "未收到Hello响应，响应大小=%d, 响应码=%d, 状态: %s", 
            bRsp.size(), bRsp.size() > 0 ? bRsp[0] : -1, 
            getErrorDescription(status).c_str());
        LDEBUG("Dload::ConnectToFlashProg", "等待500ms后重试... (%d/10)", i + 1);
        Sleep(500);
    }

    // 清除任何陈旧数据，以便从新缓冲区开始
    sport->Flush();
    if (i == 10) {
        LWARN("Dload::ConnectToFlashProg", "连接到烧录内核失败，未收到Hello响应");
        return ERROR_INVALID_HANDLE;
    }

    LDEBUG("Dload::ConnectToFlashProg", "发送Security请求");
    for (; i < 10; i++) {
        status = sport->SendSync(bSecurity, bRsp);
        if ((bRsp.size() > 0) && (bRsp[0] == EHOST_SECURITY_RSP)) {
            LDEBUG("Dload::ConnectToFlashProg", "已接收到Security响应，连接到烧录内核成功");
            return ERROR_SUCCESS;
        }
        LDEBUG("Dload::ConnectToFlashProg",
                "未收到Security响应，响应大小=%d, 响应码=0x%02x (EHOST_SECURITY_RSP=0x%02x), \n"
                "状态: %s，等待500ms后重试... (%d/10)", 
            bRsp.size(), bRsp.size() > 0 ? bRsp[0] : -1, EHOST_SECURITY_RSP,
            getErrorDescription(status).c_str(),
            i + 1
        );
        sport->Flush();
        Sleep(500);
    }
    LWARN("Dload::ConnectToFlashProg", "连接到烧录内核失败，未收到Security响应");
    return ERROR_INVALID_HANDLE;
}

int Dload::DeviceReset() {
    ByteArray rsp;
    int status = ERROR_SUCCESS;
    status = sport->SendSync(ByteArray({ EHOST_RESET_REQ }), rsp);
    if ((rsp.size() == 0) || (rsp[0] != EHSOT_RESET_ACK) || status != ERROR_SUCCESS) {
        LWARN("Dload::DeviceReset", 
                "设备重置失败，rsp.size() = %d，rsp[0] = %02x (EHSOT_RESET_ACK=0x%02x)，\n"
                "status: %s，return ERROR_INVALID_HANDLE",
            rsp.size(), rsp.size() > 0 ? rsp[0] : -1, EHSOT_RESET_ACK, 
            getErrorDescription(status).c_str());
        return ERROR_INVALID_HANDLE;
    }
    return ERROR_SUCCESS;
}

int Dload::LoadPartition(std::string szPrtnFile) {
    HANDLE hFile;
    int status = ERROR_SUCCESS;
    BYTE rsp[32];
    DWORD bytesRead = 0;
    int rspSize;
    BYTE partition[1040] = { EHOST_PARTITION_REQ, 0 };

    hFile = CreateFileA(szPrtnFile.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return ERROR_INVALID_HANDLE;

    if (!ReadFile(hFile, &partition[2], 1024, &bytesRead, NULL)) {
        status = GetLastError();
    }

    if (bytesRead > 0) {
        rspSize = sizeof(rsp);
        sport->SendSync(partition, bytesRead + 2, rsp, &rspSize);
        if ((rspSize == 0) || (rsp[0] != EHOST_PARTITION_RSP)) {
            status = ERROR_WRITE_FAULT;
        }
    }
    CloseHandle(hFile);

    if (status == ERROR_HANDLE_EOF) status = ERROR_SUCCESS;
    return status;
}

int Dload::OpenPartition(int partition) {
    BYTE openmulti[] = { EHOST_OPEN_MULTI_REQ, (BYTE) partition };
    BYTE rsp[32];
    int rspSize;

    // Flush out any previous transactions
    sport->Flush();

    // Open the partition
    rspSize = sizeof(rsp);
    sport->SendSync(openmulti, sizeof(openmulti), rsp, &rspSize);
    if ((rspSize == 0) || (rsp[0] != EHOST_OPEN_MULTI_RSP)) {
        LWARN("Dload::OpenPartition", "无法打开分区");
        return ERROR_INVALID_HANDLE;
    }

    return ERROR_SUCCESS;
}

int Dload::ClosePartition() {
    BYTE close[] = { EHOST_CLOSE_REQ };
    BYTE rsp[32];
    int rspSize;

    // Close the partition we were writing to
    rspSize = sizeof(rsp);
    sport->SendSync(close, sizeof(close), rsp, &rspSize);
    if ((rspSize == 0) || (rsp[0] != EHOST_CLOSE_RSP)) {
        return ERROR_INVALID_HANDLE;
    }
    return ERROR_SUCCESS;
}

int Dload::FastCopySerial(HANDLE hInFile, uint32_t offset, uint32_t sectors) {
    int status = ERROR_SUCCESS;
    BYTE rsp[32];
    DWORD bytesRead = 0;
    uint32_t count = 0;
    uint32_t readSize = 1024;
    int rspSize;
    BYTE streamwrite[1050] = { EHOST_STREAM_WRITE_REQ, 0 };

    // For now don't worry about sliding window do a write and wait for response
    while ((status == ERROR_SUCCESS) && (count < sectors)) {
        if ((count + 2) > sectors) {
            readSize = 512;
        }
        // If HANDLE value is invalid then just write 0's
        if (hInFile != INVALID_HANDLE_VALUE) {
            if (!ReadFile(hInFile, &streamwrite[5], readSize, &bytesRead, NULL)) {
                status = GetLastError();
            }
        } else {
            memset(&streamwrite[5], 0, readSize);
            bytesRead = readSize;
        }

        count += (bytesRead / 512);
        if (bytesRead > 0) {
            // Write out the bytes we read to destination
            streamwrite[1] = offset & 0xff;
            streamwrite[2] = (offset >> 8) & 0xff;
            streamwrite[3] = (offset >> 16) & 0xff;
            streamwrite[4] = (offset >> 24) & 0xff;
            rspSize = sizeof(rsp);

            sport->SendSync(streamwrite, bytesRead + 5, rsp, &rspSize);
            if ((rspSize == 0) || (rsp[0] != EHOST_STREAM_WRITE_RSP)) {
                LWARN("Dload::FastCopySerial", "设备返回错误: %d", rsp[0]);
                if (rsp[0] == EHOST_LOG && rspSize >= 2) {
                    rsp[rspSize - 2] = 0;
                    LDEBUG("Dload::FastCopySerial", "设备日志: %s", (char*) &rsp[1]);
                }
                status = ERROR_WRITE_FAULT;
            }
            if (bSectorAddress) {
                offset += bytesRead / SECTOR_SIZE;
                LDEBUG("Dload::FastCopySerial", "目标扇区: %d", (int) offset);
            } else {
                offset += bytesRead;
                LDEBUG("Dload::FastCopySerial", "目标偏移: %d", (int) offset);
            }
        } else {
            // If we didn't read anything then break
            break;
        }
    }

    LDEBUG("Dload::FastCopySerial", "传输完成");

    // If we hit end of file that means we sent it all
    if (status == ERROR_HANDLE_EOF) status = ERROR_SUCCESS;
    return status;
}

int Dload::LoadImageFile(const std::string& szSingleImage) {
    HANDLE hFile;
    int status = ERROR_SUCCESS;

    hFile = CreateFileA(szSingleImage.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        LWARN("Dload::LoadImageFile", "文件未找到: %s", szSingleImage.c_str());
        return ERROR_INVALID_HANDLE;
    }

    status = FastCopySerial(hFile, 0, (uint32_t) -1);
    CloseHandle(hFile);

    return status;
}

int Dload::LoadFlashProg(const std::string& szFlashPrg) {
    BYTE write32[280] = { CMD_WRITE_32BIT, 0 };
    BYTE* writePtr;
    BYTE rsp[32];
    int rspSize;
    uint32_t goAddr = 0;
    uint32_t targetAddr = 0;
    uint32_t newAddr = 0;
    uint32_t bytesRead;
    uint32_t status = ERROR_SUCCESS;

    char hexline[128];
    FILE* fMprgFile;
    fopen_s(&fMprgFile, szFlashPrg.c_str(), "r");

    if (fMprgFile == NULL) {
        return ERROR_OPEN_FAILED;
    }

    while (!feof(fMprgFile)) {
        BYTE id = 0;
        bool bFirst = true;
        writePtr = &write32[7];
        for (bytesRead = 0; bytesRead < 256;) {
            BYTE len;
            fgets(hexline, sizeof(hexline), fMprgFile);
            if (strlen(hexline) > 9) {
                HexTobyte(&hexline[1], &len, 1);
                HexTobyte(&hexline[3], rsp, 2);
                HexTobyte(&hexline[7], &id, 1);

                if (id == 0) {
                    HexTobyte(&hexline[9], writePtr, len);
                    if (bFirst) {
                        bFirst = false;
                        targetAddr = (targetAddr & 0xffff0000) + (rsp[0] << 8) + rsp[1];
                    }
                    writePtr += len;
                    bytesRead += len;
                } else if (id == 1) {
                    break;
                } else if (id == 5) {
                    HexTobyte(&hexline[9], rsp, 4);
                    goAddr = (rsp[0] << 24) + (rsp[1] << 16) + (rsp[2] << 8) + rsp[3];
                } else if (id == 4) {
                    HexTobyte(&hexline[9], rsp, 2);
                    newAddr = (rsp[0] << 24) + (rsp[1] << 16);
                    break;
                }
            }

            if (id == 4) {
                break;
            }
        }


        if (bytesRead > 0) {
            write32[1] = (targetAddr >> 24) & 0xff;
            write32[2] = (targetAddr >> 16) & 0xff;
            write32[3] = (targetAddr >> 8) & 0xff;
            write32[4] = targetAddr & 0xff;
            write32[5] = (bytesRead >> 8) & 0xff;
            write32[6] = bytesRead & 0xff;
            rspSize = sizeof(rsp);
            LDEBUG("Dload::LoadMprgFile", "编程地址: 0x%x 长度: %d", targetAddr, bytesRead);
            sport->SendSync(write32, bytesRead + 7, rsp, &rspSize);
            if ((rspSize == 0) || (rsp[0] != CMD_ACK)) {
                status = ERROR_WRITE_FAULT;
                break;
            }
        }

        targetAddr = newAddr;
    }
    fclose(fMprgFile);


    if (status == ERROR_SUCCESS) {
        BYTE gocmd[5] = { CMD_GO, 0 };
        LDEBUG("Dload::LoadMprgFile", "发送GO命令到地址: 0x%x", (uint32_t) goAddr);
        gocmd[1] = (goAddr >> 24) & 0xff;
        gocmd[2] = (goAddr >> 16) & 0xff;
        gocmd[3] = (goAddr >> 8) & 0xff;
        gocmd[4] = goAddr & 0xff;

        rspSize = sizeof(rsp);
        sport->SendSync(gocmd, sizeof(gocmd), rsp, &rspSize);
        if ((rspSize > 0) && (rsp[0] == CMD_ACK)) {
            status = ERROR_SUCCESS;
        } else {
            status = ERROR_WRITE_FAULT;
        }
    }

    return status;
}

int Dload::GetDloadParams(BYTE* rsp, int len) {
    BYTE params[] = { CMD_PREQ };
    int bytesRead = len;
    // For params response we need at least 256 bytes
    if (len < 16) {
        return -1;
    }
    sport->SendSync(params, sizeof(params), rsp, &bytesRead);
    if ((bytesRead > 0) && (rsp[0] == CMD_PARAMS)) {
        return ERROR_SUCCESS;
    }
    return -2;
}

int Dload::IsDeviceInDload(void) {
    BYTE nop[] = { CMD_NOP };
    BYTE rsp[32] = { 0 };
    int bytesRead = 32;

    sport->SendSync(nop, sizeof(nop), rsp, &bytesRead);
    if (rsp[0] == CMD_ACK) {
        return ERROR_SUCCESS;
    }

    return ERROR_INVALID_HANDLE;
}

int Dload::SetActivePartition() {
    // Set currently open partition to active
    BYTE active_prtn[38] = { EHOST_STREAM_DLOAD_REQ, 0x2 };
    BYTE rsp[128] = { 0 };
    int status = ERROR_SUCCESS;
    int bytesRead = sizeof(rsp);
    unsigned short crc;
    active_prtn[34] = 0xAD;
    active_prtn[35] = 0xDE;
    crc = CalcCRC16(&active_prtn[1], 35);
    active_prtn[36] = crc & 0xff;
    active_prtn[37] = crc >> 8;
    status = sport->SendSync(active_prtn, sizeof(active_prtn), rsp, &bytesRead);
    if ((status == ERROR_SUCCESS) && (rsp[0] == EHOST_STREAM_DLOAD_RSP)) {
        return ERROR_SUCCESS;
    }

    return status;
}

int Dload::CreateGPP(uint32_t dwGPP1, uint32_t dwGPP2, uint32_t dwGPP3, uint32_t dwGPP4) {
    BYTE stream_dload[38] = { EHOST_STREAM_DLOAD_REQ, 0x1 };
    BYTE rsp[128] = { 0 };
    int bytesRead = sizeof(rsp);
    int status = ERROR_SUCCESS;
    unsigned short crc;
    stream_dload[2] = dwGPP1 & 0xff;
    stream_dload[3] = (dwGPP1 >> 8) & 0xff;
    stream_dload[4] = (dwGPP1 >> 16) & 0xff;
    stream_dload[5] = (dwGPP1 >> 24) & 0xff;
    stream_dload[6] = dwGPP2 & 0xff;
    stream_dload[7] = (dwGPP2 >> 8) & 0xff;
    stream_dload[8] = (dwGPP2 >> 16) & 0xff;
    stream_dload[9] = (dwGPP2 >> 24) & 0xff;
    stream_dload[10] = dwGPP3 & 0xff;
    stream_dload[11] = (dwGPP3 >> 8) & 0xff;
    stream_dload[12] = (dwGPP3 >> 16) & 0xff;
    stream_dload[13] = (dwGPP3 >> 24) & 0xff;
    stream_dload[14] = dwGPP4 & 0xff;
    stream_dload[15] = (dwGPP4 >> 8) & 0xff;
    stream_dload[16] = (dwGPP4 >> 16) & 0xff;
    stream_dload[17] = (dwGPP4 >> 24) & 0xff;
    stream_dload[34] = 0xAD;
    stream_dload[35] = 0xDE;
    crc = CalcCRC16(&stream_dload[1], 35);
    stream_dload[36] = crc & 0xff;
    stream_dload[37] = crc >> 8;
    status = sport->SendSync(stream_dload, sizeof(stream_dload), rsp, &bytesRead);
    if ((status == ERROR_SUCCESS) && (rsp[0] == EHOST_STREAM_DLOAD_RSP)) {
        // Return actual size of the card
        return ERROR_SUCCESS;
    }
    return status;
}


uint64_t Dload::GetNumDiskSectors() {
    BYTE stream_dload[38] = { EHOST_STREAM_DLOAD_REQ, 0x0 };
    BYTE rsp[128] = { 0 };
    int bytesRead = sizeof(rsp);
    int status = ERROR_SUCCESS;
    unsigned short crc;
    stream_dload[34] = 0xAD;
    stream_dload[35] = 0xDE;
    crc = CalcCRC16(&stream_dload[1], 35);
    stream_dload[36] = crc & 0xff;
    stream_dload[37] = crc >> 8;
    status = sport->SendSync(stream_dload, sizeof(stream_dload), rsp, &bytesRead);
    if ((status == ERROR_SUCCESS) && (rsp[0] == EHOST_STREAM_DLOAD_RSP)) {
        // Return actual size of the card
        return (rsp[5] | rsp[6] << 8 | rsp[7] << 16 | rsp[8] << 24);
    }
    return status;
}

int Dload::ProgramPartitionEntry(PartitionEntry pe) {
    int status = ERROR_SUCCESS;
    HANDLE hInFile = INVALID_HANDLE_VALUE;

    if (pe.filename == "ZERO") {
        LDEBUG("Dload::ProgramPartitionEntry", "清零区域");
    } else {
        hInFile = CreateFileA(pe.filename.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL);
        if (hInFile == INVALID_HANDLE_VALUE) {
            status = GetLastError();
            LERROR("Dload::ProgramPartitionEntry", "无法打开文件: %s, 错误码: %d", pe.filename.c_str(), status);
        } else {
            LONG dwOffsetHigh = (pe.offset >> 32);
            status = SetFilePointer(hInFile, (LONG) pe.offset, &dwOffsetHigh, FILE_BEGIN);
            if (status == INVALID_SET_FILE_POINTER) {
                status = GetLastError();
                LERROR("Dload::ProgramPartitionEntry", "设置文件指针失败，错误码: %d", status);
            } else {
                status = ERROR_SUCCESS;
            }
        }
    }

    if (status == ERROR_SUCCESS) {
        LDEBUG("Dload::ProgramPartitionEntry", "输入偏移: %lld 输出偏移: %lld 扇区数: %lld", pe.offset, pe.start_sector, pe.num_sectors);
        status = FastCopySerial(hInFile, (uint32_t) pe.start_sector, (uint32_t) pe.num_sectors);
    }

    if (hInFile != INVALID_HANDLE_VALUE) CloseHandle(hInFile);
    return status;
}

int Dload::WipeDiskContents(uint64_t start_sector, uint64_t num_sectors) {
    PartitionEntry pe;
    memset(&pe, 0, sizeof(pe));
    pe.filename = "ZERO";
    pe.start_sector = start_sector;
    pe.num_sectors = num_sectors;
    return ProgramPartitionEntry(pe);
}

int Dload::WriteRawProgramFile(const std::string& szXMLFile) {
    int status = ERROR_SUCCESS;
    Partition* p;
    p = new Partition();

    LDEBUG("Dload::ProgramPartitions", "解析分区表: %s", szXMLFile.c_str());
    status = p->PreLoadImage(szXMLFile);
    if (status != ERROR_SUCCESS) {
        LWARN("Dload::ProgramPartitions", "分区表加载失败");
        delete p;
        return status;
    }

    PartitionEntry pe;
    std::string keyName;
    std::string key;
    while (p->GetNextXMLKey(keyName, key) == ERROR_SUCCESS) {
        if (p->ParseXMLKey(key, &pe) != ERROR_SUCCESS) continue;
        if (pe.eCmd == CMD_PROGRAM) {
            status = ProgramPartitionEntry(pe);
        } else if (pe.eCmd == CMD_PATCH) {
            Partition* part = new Partition();
            part->ProgramPartitionEntry(nullptr, pe, "");
            delete part;
        } else if (pe.eCmd == CMD_ZEROOUT) {
            status = WipeDiskContents(pe.start_sector, pe.num_sectors);
        }

        if (status != ERROR_SUCCESS) {
            break;
        }
    }

    p->CloseXML();
    delete p;
    return status;
}
