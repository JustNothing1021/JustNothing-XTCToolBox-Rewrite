/*****************************************************************************
 * firehose.cpp
 *
 * This class can perform emergency flashing given the transport layer
 *
 * Copyright (c) 2007-2013
 * Qualcomm Technologies Incorporated.
 * All Rights Reserved.
 * Qualcomm Confidential and Proprietary
 *
 *****************************************************************************/
 /*=============================================================================
                         Edit History

 $Header: //source/qcom/qct/platform/uefi/workspaces/pweber/apps/8x26_emmcdl/emmcdl/main/latest/src/firehose.cpp#20 $
 $DateTime: 2015/05/07 21:41:17 $ $Author: pweber $

 when       who     what, where, why
 -------------------------------------------------------------------------------
 02/06/13   pgw     Initial version.
 =============================================================================*/

#include "stdio.h"
#include "utils/time_utils.h"
#include "utils/logger.h"
#include "utils/string_utils.h"
#include "emmcdl_new/firehose.h"
#include "emmcdl_new/xmlparser.h"
#include "emmcdl_new/utils.h"
#include "emmcdl_new/partition.h"
#include <exception>

Firehose::~Firehose() {
    if (m_payload != NULL) {
        free(m_payload);
        m_payload = NULL;
    }
    if (program_pkt != NULL) {
        free(program_pkt);
        program_pkt = NULL;
    }
}

Firehose::Firehose(SerialPort* port, HANDLE hLogFile) {
    bSectorAddress = true;
    dwMaxPacketSize = 1024 * 1024;
    diskSectors = 0;
    hLog = hLogFile;
    sport = port;
    m_payload = NULL;
    program_pkt = NULL;
    m_buffer_len = 0;
    m_buffer_ptr = NULL;
}

int Firehose::ReadData(BYTE* pOutBuf, DWORD dwBufSize, bool bXML) {
    BYTE* origPOutBuf = pOutBuf;
    DWORD dwBytesRead = 0;
    bool bFoundXML = false;
    int status = ERROR_SUCCESS;
    LTRACE("Firehose::ReadData", "尝试读取数据, 缓存至%p, 大小为%d bytes", (void*) pOutBuf, dwBufSize);
    if (bXML) {
        memset(pOutBuf, 0, dwBufSize);
        for (int i = 0; i < 3; i++) {
            for (; m_buffer_len > 0; m_buffer_len--) {
                *pOutBuf++ = *m_buffer_ptr++;
                dwBytesRead++;

                if (strncmp(((char*) pOutBuf) - 7, "</data>", 7) == 0) {
                    m_buffer_len--;
                    bFoundXML = true;
                    break;
                }
            }

            if (!bFoundXML) {
                memset(m_buffer, 0, dwMaxPacketSize);
                m_buffer_len = dwMaxPacketSize;
                m_buffer_ptr = m_buffer;
                if (sport->Read(m_buffer, &m_buffer_len) != ERROR_SUCCESS) {
                    m_buffer_len = 0;
                }
            } else {
                break;
            }
        }
    } else {
        LDEBUG("Firehose::ReadData", "缓冲数据剩余量: %d bytes", m_buffer_len);
        if (m_buffer_len >= dwBufSize) {
            m_buffer_len -= dwBufSize;
            LDEBUG("Firehose::ReadData", "当前缓冲区内的数据足以满足读取要求");

            memcpy_s(pOutBuf, dwBufSize, m_buffer_ptr, dwBufSize);
            m_buffer_ptr += dwBufSize;
            if (dwBufSize >= 2048) {
                LTRACE("Firehose::ReadData", "读取到的数据包: \n<数据过长(%d bytes)暂不显示>", dwBufSize);
            } else {
                LTRACE("Firehose::ReadData", "读取到的数据包: \n%s\n(%d bytes)", 
                            string_utils::to_hex_view((char*) origPOutBuf, dwBufSize), dwBufSize);
            }
            return dwBytesRead;
        }

        memcpy_s(pOutBuf, dwBufSize, m_buffer_ptr, m_buffer_len);
        pOutBuf += m_buffer_len;
        dwBytesRead = dwBufSize - m_buffer_len;
    }

    status = sport->Read(pOutBuf, &dwBytesRead);
    dwBytesRead += m_buffer_len;
    m_buffer_len = 0;

    if (status != ERROR_SUCCESS) {
        LWARN("Firehose::ReadData", "读取设备响应数据包失败: %s", getErrorDescription(status).c_str());
        return 0;
    }
    if (dwBytesRead >= 2048) {
        LTRACE("Firehose::ReadData", "读取到的数据包: \n<数据过长(%d bytes)暂不显示>", dwBytesRead);
    } else {
        LTRACE("Firehose::ReadData", "读取到的数据包: \n%s\n(%d bytes)", 
                    string_utils::to_hex_view((char*) origPOutBuf, dwBytesRead), dwBytesRead);
    }
    return dwBytesRead;
}

int Firehose::ConnectToFlashProg(fh_configure_t* cfg) {
    int status = ERROR_SUCCESS;
    DWORD dwBytesRead = dwMaxPacketSize;
    DWORD retry = 0;

    if (m_payload == NULL || program_pkt == NULL || m_buffer == NULL) {
        m_payload = (BYTE*) malloc(dwMaxPacketSize);
        program_pkt = (char*) malloc(MAX_XML_LEN);
        m_buffer = (BYTE*) malloc(dwMaxPacketSize);
        if (m_payload == NULL || program_pkt == NULL || m_buffer == NULL) {
            return ERROR_OUTOFMEMORY;
        }
    }
    memset(m_payload, 0, dwMaxPacketSize);
    dwBytesRead = ReadData((BYTE*) m_payload, dwMaxPacketSize, false);

    if ((_stricmp(cfg->MemoryName, "ufs") == 0) && (DISK_SECTOR_SIZE == 512)) {
        LDEBUG("Firehose::ConnectToFlashProg", "使用扇区大小 SECTOR_SIZE=4096 来操作UFS设备");
        DISK_SECTOR_SIZE = 4096;
    } else {
        LDEBUG("Firehose::ConnectToFlashProg", "使用扇区大小 SECTOR_SIZE=%d 来操作设备", DISK_SECTOR_SIZE);
    }

    sprintf_s(program_pkt, MAX_XML_LEN,
        "<?xml version=\"1.0\" ?>\n"
        "<data>\n"
        "    <configure MemoryName=\"%s\" ZLPAwareHost=\"%d\" SkipStorageInit=\"%d\" SkipWrite=\"%d\" MaxPayloadSizeToTargetInBytes=\"%d\"/>\n"
        "</data>\n",
        cfg->MemoryName, cfg->ZLPAwareHost, cfg->SkipStorageInit, cfg->SkipWrite, dwMaxPacketSize);
    LTRACE("Firehose::ConnectToFlashProg", "正在发送配置数据包: \n%s", 
        string_utils::to_hex_view((char*) program_pkt, strnlen_s(program_pkt, MAX_XML_LEN)));
    status = sport->Write((BYTE*) program_pkt, strnlen_s(program_pkt, MAX_XML_LEN));
    if (status == ERROR_SUCCESS) {
        for (; retry < MAX_RETRY; retry++) {
            status = ReadStatus();
            if (status == ERROR_SUCCESS) {
                break;
            } else if (status == ERROR_INVALID_DATA) {
                XMLParser xmlparse;
                uint64_t u64MaxSize = 0;
                xmlparse.ParseXMLInteger((char*) m_payload, "MaxPayloadSizeToTargetInBytes", &u64MaxSize);

                if ((u64MaxSize > 0) && (u64MaxSize < dwMaxPacketSize)) {
                    LDEBUG("Firehose::ConnectToFlashProg", "由于设备无法处理这么大的数据包(当前为%d, 设备允许的为%d)，将最大数据包大小降低为%d",
                        dwMaxPacketSize, (int) u64MaxSize, (int) u64MaxSize);
                    dwMaxPacketSize = (uint32_t) u64MaxSize;
                    return ConnectToFlashProg(cfg);
                }
            }
        }

        if (retry == MAX_RETRY) {
            LWARN("Firehose::ConnectToFlashProg", "错误: 设备未响应配置数据包 (固定返回 %d)", ERROR_NOT_READY);
            return ERROR_NOT_READY;
        }
    }

    LTRACE("Firehose::ConnectToFlashProg", "读取设备响应数据包...");
    dwBytesRead = ReadData((BYTE*) m_payload, dwMaxPacketSize, false);
    LTRACE("Firehose::ConnectToFlashProg", "读取到的数据包: \n%s", 
        string_utils::to_hex_view((char*) m_payload, dwBytesRead));
    return status;
}

int Firehose::DeviceReset() {
    int status = ERROR_SUCCESS;
    char reset_pkt[] = "<?xml version=\"1.0\" ?>\n"
        "<data>\n"
        "    <power value=\"reset\"/>\n"
        "</data>";
    LDEBUG("Firehose::DeviceReset", "重置设备");
    LTRACE("Firehose::DeviceReset", "正在发送的数据包: \n%s", string_utils::to_hex_view((string) reset_pkt));
    status = sport->Write((BYTE*) reset_pkt, sizeof(reset_pkt));
    if (status != ERROR_SUCCESS)
        LWARN("Firehose::DeviceReset", "发送重置命令时出现错误: %s", getErrorDescription(status).c_str());
    return status;
}

int Firehose::ReadStatus(void) {
    LTRACE("Firehose::ReadStatus", "检查设备状态");
    while (ReadData(m_payload, MAX_XML_LEN, true) > 0) {
        if (strstr((char*) m_payload, "ACK") != NULL) {
            LTRACE("Firehose::ReadStatus", "设备状态正常");
            return ERROR_SUCCESS;
        } else if (strstr((char*) m_payload, "NAK") != NULL) {
            LWARN("Firehose::ReadStatus", "检查设备状态出现异常，设备返回了NAK");
            return ERROR_INVALID_DATA;
        }
    }
    LWARN("Firehose::ReadStatus", "检查设备状态出现异常，设备未响应 (固定返回 %d)", ERROR_NOT_READY);
    return ERROR_NOT_READY;
}

int Firehose::ProgramPatchEntry(PartitionEntry pe, const std::string& key) {
    UNREFERENCED_PARAMETER(pe);
    char tmp_key[MAX_STRING_LEN];
    int status = ERROR_SUCCESS;

    if (key.empty())
        return ERROR_INVALID_PARAMETER;
    strcpy_s(tmp_key, key.c_str());

    memset(program_pkt, 0, MAX_XML_LEN);
    sprintf_s(program_pkt, MAX_XML_LEN, "<?xml version=\"1.0\" ?><data>");
    StringReplace(tmp_key, ".", "");
    StringReplace(tmp_key, ".", "");
    strcat_s(program_pkt, MAX_XML_LEN, tmp_key);
    strcat_s(program_pkt, MAX_XML_LEN, "></data>\n");
    LTRACE("Firehose::ProgramPatchEntry", "正在发送的数据包: \n%s", 
        string_utils::to_hex_view(string((char*) program_pkt)));
    status = sport->Write((BYTE*) program_pkt, strlen(program_pkt));
    if (status != ERROR_SUCCESS) {
        LWARN("Firehose::ProgramPatchEntry", "发送数据包时出现错误: %s", getErrorDescription(status).c_str());
        return status;
    }

    int status2 = ReadStatus();
    if (status2 != ERROR_SUCCESS) {
        LWARN("Firehose::ProgramPatchEntry", "检查设备状态出现异常: %s", getErrorDescription(status2).c_str());
        return status2;
    }
    return status;
}

int Firehose::WriteData(BYTE* writeBuffer, int64_t writeOffset, DWORD writeBytes, DWORD* bytesWritten, uint8_t partNum) {
    DWORD dwBytesRead;
    int status = ERROR_SUCCESS;

    if (writeBuffer == NULL || bytesWritten == NULL) {
        LWARN(
            "Firehose::WriteData",
            "写入数据时参数错误, 请检查调用参数 (这一般是代码错误, 数据: writeBuffer=%p, bytesWritten=%p)",
            (void*) writeBuffer, (void*) bytesWritten
        );
        return ERROR_INVALID_PARAMETER;
    }

    *bytesWritten = 0;
    memset(program_pkt, 0, MAX_XML_LEN);
    if (writeOffset >= 0) {
        sprintf_s(program_pkt, MAX_XML_LEN,
            "<?xml version=\"1.0\" ?>\n"
            "<data>\n"
            "<program SECTOR_SIZE_IN_BYTES=\"%d\" num_partition_sectors=\"%d\" physical_partition_number=\"%d\" start_sector=\"%d\"/>\n"
            "</data>\n",
            DISK_SECTOR_SIZE, (int) writeBytes / DISK_SECTOR_SIZE, partNum, (int) (writeOffset / DISK_SECTOR_SIZE));
    } else {
        sprintf_s(program_pkt, MAX_XML_LEN,
            "<?xml version=\"1.0\" ?>\n"
            "<data>\n"
            "<program SECTOR_SIZE_IN_BYTES=\"%d\" num_partition_sectors=\"%d\" physical_partition_number=\"%d\" start_sector=\"NUM_DISK_SECTORS%d\"/>\n"
            "</data>\n",
            DISK_SECTOR_SIZE, (int) writeBytes / DISK_SECTOR_SIZE, partNum, (int) (writeOffset / DISK_SECTOR_SIZE));
    }

    LTRACE("Firehose::WriteData", "正在发送的数据包: \n%s", string_utils::to_hex_view(string((char*) program_pkt)));
    status = sport->Write((BYTE*) program_pkt, strlen(program_pkt));

    double st = time_utils::get_time();

    if (ReadStatus() != ERROR_SUCCESS)
        goto WriteSectorsExit;

    dwBytesRead = dwMaxPacketSize;
    for (DWORD i = 0; i < writeBytes; i += dwMaxPacketSize) {
        if ((writeBytes - i) < dwMaxPacketSize) {
            dwBytesRead = (int) (writeBytes - i);
        }
        status = sport->Write(&writeBuffer[i], dwBytesRead);
        if (status != ERROR_SUCCESS) {
            goto WriteSectorsExit;
        }
        *bytesWritten += dwBytesRead;
        LTRACE("Firehose::WriteData", "已写入%10d/%10dbytes, 剩余%10dbytes (%.3f%%)",
            (int) i, (int) writeBytes,
            (int) (writeBytes - i),
            (double) i / writeBytes * 100.0
        );
    }

    LDEBUG("Firehose::WriteData", "写入完成，总大小%d字节, 速度%.3fKB/s",
        (int) writeBytes, (double) ((writeBytes / 1024.0) / (max(time_utils::get_time() - st, 0.0001))));

    LDEBUG("Firehose::WriteData", "等待设备响应");
    status = ReadStatus();
    LDEBUG("Firehose::WriteData", "设备响应完成, 状态: %s",
        getErrorDescription(status).c_str());

WriteSectorsExit:
    return status;
}

int Firehose::ReadData(BYTE* readBuffer, int64_t readOffset, DWORD readBytes, DWORD* bytesRead, uint8_t partNum) {
    LTRACE("Firehose::ReadData", "正在读取数据, 起始偏移量: %llu, 预计读取大小: %llu, 分区号: %d, 数据和实际长度保存至%p和%p", 
        readOffset, readBytes, partNum, (void*) readBuffer, (void*) bytesRead);
    DWORD dwBytesRead;
    int status = ERROR_SUCCESS;

    if (readBuffer == NULL || bytesRead == NULL) {
        return ERROR_INVALID_PARAMETER;
    }

    memset(program_pkt, 0, MAX_XML_LEN);
    if (readOffset >= 0) {
        sprintf_s(program_pkt, MAX_XML_LEN,
            "<?xml version=\"1.0\" ?>\n"
            "<data>\n"
            "<read SECTOR_SIZE_IN_BYTES=\"%d\" num_partition_sectors=\"%d\" physical_partition_number=\"%d\" start_sector=\"%d\"/>\n"
            "</data>\n",
            DISK_SECTOR_SIZE, (int) readBytes / DISK_SECTOR_SIZE, partNum, (int) (readOffset / DISK_SECTOR_SIZE));
    } else {
        sprintf_s(program_pkt, MAX_XML_LEN,
            "<?xml version=\"1.0\" ?>\n"
            "<data>\n"
            "<read SECTOR_SIZE_IN_BYTES=\"%d\" num_partition_sectors=\"%d\" physical_partition_number=\"%d\" start_sector=\"NUM_DISK_SECTORS%d\"/>\n"
            "</data>\n",
            DISK_SECTOR_SIZE, (int) readBytes / DISK_SECTOR_SIZE, partNum, (int) (readOffset / DISK_SECTOR_SIZE));
    }

    LTRACE("Firehose::ReadData", "正在发送的数据包: \n%s", string_utils::to_hex_view(string((char*) program_pkt)));

    status = sport->Write((BYTE*) program_pkt, strlen(program_pkt));

    while ((status = ReadStatus()) == ERROR_NOT_READY);
    double st = time_utils::get_time();
    DWORD bytesToRead = dwMaxPacketSize;

    if (status == ERROR_INVALID_DATA)
        goto ReadSectorsExit;

    for (uint32_t tmp_sectors = (uint32_t) readBytes / DISK_SECTOR_SIZE; tmp_sectors > 0; tmp_sectors -= (bytesToRead / DISK_SECTOR_SIZE)) {
        if (tmp_sectors < dwMaxPacketSize / DISK_SECTOR_SIZE) {
            bytesToRead = tmp_sectors * DISK_SECTOR_SIZE;
        } else {
            bytesToRead = dwMaxPacketSize;
        }

        DWORD offset = 0;
        while (offset < bytesToRead) {
            dwBytesRead = ReadData(&readBuffer[offset], bytesToRead - offset, false);
            offset += dwBytesRead;
        }

        readBuffer += bytesToRead;
        *bytesRead += bytesToRead;
        LTRACE("Firehose::ReadData", "已读取%10d/%10dbytes, 剩余%10dbytes (%.3f%%)",
            (int) (*bytesRead), (int) readBytes,
            (int) (tmp_sectors * DISK_SECTOR_SIZE - bytesToRead),
            (double) (*bytesRead) / readBytes * 100.0
        );
    }

    LDEBUG("Firehose::ReadData", "读取完成, 总大小%d字节, 速度%.3fKB/s",
        (int) readBytes, (double) ((readBytes / 1024.0) / max(time_utils::get_time() - st, 0.0001)));

    LTRACE("Firehose::ReadData", "等待设备响应");
    status = ReadStatus();
    LTRACE("Firehose::ReadData", "设备响应完成, 状态: %s",
        getErrorDescription(status).c_str());

ReadSectorsExit:
    if (status != ERROR_SUCCESS) {
        LWARN("Firehose::ReadData", "读取数据遇到错误, 状态: %s",
            getErrorDescription(status).c_str());
    } else {
        if (*bytesRead <= 1024) {
            LTRACE("Firehose::ReadData", "读取到的数据包: \n%s\n(%d bytes)", 
                string_utils::to_hex_view((char*) readBuffer, *bytesRead), *bytesRead);
        } else {
            LTRACE("Firehose::ReadData", "读取到的数据包: \n(数据过长无法展示, %d bytes)", *bytesRead);
        }
    }
    return status;
}

int Firehose::WriteData(const ByteArray& writeBuffer, int64_t writeOffset, DWORD* bytesWritten, uint8_t partNum) {
    return WriteData((BYTE*)writeBuffer.data(), writeOffset, writeBuffer.size(), bytesWritten, partNum);
}

int Firehose::ReadData(ByteArray& readBuffer, int64_t readOffset, DWORD readBytes, DWORD* bytesRead, uint8_t partNum) {
    readBuffer.resize(readBytes);
    int status = ReadData(readBuffer.data(), readOffset, readBytes, bytesRead, partNum);
    if (status == ERROR_SUCCESS && *bytesRead < readBytes) {
        readBuffer.resize(*bytesRead);
    }
    return status;
}

int Firehose::CreateGPP(DWORD dwGPP1, DWORD dwGPP2, DWORD dwGPP3, DWORD dwGPP4) {
    int status = ERROR_SUCCESS;

    sprintf_s(program_pkt, MAX_XML_LEN,
        "<?xml version=\"1.0\" ?>\n"
        "<data>\n"
        "<createstoragedrives DRIVE4_SIZE_IN_KB=\"%d\" DRIVE5_SIZE_IN_KB=\"%d\" DRIVE6_SIZE_IN_KB=\"%d\" DRIVE7_SIZE_IN_KB=\"%d\" />\n"
        "</data>",
        dwGPP1, dwGPP2, dwGPP3, dwGPP4);

    LTRACE("Firehose::CreateGPP", "正在发送的数据包: \n%s", string_utils::to_hex_view(string((char*) program_pkt)));

    status = sport->Write((BYTE*) program_pkt, strlen(program_pkt));

    status = ReadStatus();

    if (ReadData(m_payload, dwMaxPacketSize, false) > 0)
        LTRACE("Firehose::CreateGPP", "设备响应: \n%s", (char*) m_payload);

    return status;
}

int Firehose::SetActivePartition(int prtn_num) {
    int status = ERROR_SUCCESS;
    LDEBUG("Firehose::SetActivePartition", "尝试设置活动分区为%d", prtn_num);

    sprintf_s(program_pkt, MAX_XML_LEN,
        "<?xml version=\"1.0\" ?>"
        "<data>\n"
        "    <setbootablestoragedrive value=\"%d\" />\n"
        "</data>\n",
        prtn_num);

    LTRACE("Firehose::SetActivePartition", "正在发送的数据包:\n%s", string_utils::to_hex_view(string((char*) program_pkt)));
    status = sport->Write((BYTE*) program_pkt, strlen(program_pkt));

    LTRACE("Firehose::SetActivePartition", "等待设备响应");
    status = ReadStatus();
    LDEBUG("Firehose::SetActivePartition", "设备响应完成, 状态: %s",
        getErrorDescription(status).c_str());

    if (ReadData(m_payload, dwMaxPacketSize, false) > 0)
        LTRACE("Firehose::SetActivePartition", "设备响应: \n%s", (char*) m_payload);

    return status;
}

int Firehose::ProgramRawCommand(const std::string& key) {
    DWORD dwBytesRead;
    int status = ERROR_SUCCESS;
    memset(program_pkt, 0, MAX_XML_LEN);
    sprintf_s(program_pkt, MAX_XML_LEN, "<?xml version=\"1.0\" ?><data>");
    strcat_s(program_pkt, MAX_XML_LEN, key.c_str());
    strcat_s(program_pkt, MAX_XML_LEN, "></data>\n");
    LTRACE("Firehose::ProgramRawCommand", "正在发送的数据包: \n%s", string_utils::to_hex_view((string) (char*) program_pkt));
    status = sport->Write((BYTE*) program_pkt, strlen(program_pkt));

    dwBytesRead = ReadData(m_payload, dwMaxPacketSize, false);
    LTRACE("Firehose::ProgramRawCommand", "设备响应: \n%s", (char*) m_payload);

    return status;
}

int Firehose::FastCopy(HANDLE hRead, int64_t sectorRead, HANDLE hWrite, int64_t sectorWrite, uint64_t sectors, uint8_t partNum) {
    DWORD dwBytesRead = 0;
    BOOL bReadStatus = TRUE;
    int64_t dwWriteOffset = sectorWrite * DISK_SECTOR_SIZE;
    int64_t dwReadOffset = sectorRead * DISK_SECTOR_SIZE;
    int status = ERROR_SUCCESS;

    LDEBUG("Firehose::FastCopy", "开始快速拷贝: 读取句柄=%p, 读取偏移=0x%llx, 写入句柄=%p, 写入偏移=0x%llx, 扇区数=%llu, 分区号=%d",
        hRead, sectorRead, hWrite, sectorWrite, sectors, partNum);

    if (hWrite == NULL) {
        return ERROR_INVALID_PARAMETER;
    }

    if (hRead == INVALID_HANDLE_VALUE) {
        LDEBUG("Firehose::FastCopy", "读取句柄无效, 将输入缓冲区清零");
        memset(m_payload, 0, dwMaxPacketSize);
        dwBytesRead = dwMaxPacketSize;
    } else {
        if (sectorRead > 0) {
            LONG sectorReadHigh = dwReadOffset >> 32;
            status = SetFilePointer(hRead, (LONG) dwReadOffset, &sectorReadHigh, FILE_BEGIN);
            if (status == INVALID_SET_FILE_POINTER) {
                status = GetLastError();
                LERROR("Firehose::FastCopy", "设置读取偏移0x%llx失败, 状态: %s",
                    sectorRead, getErrorDescription(status).c_str());
                return status;
            }
        }
    }

    memset(program_pkt, 0, MAX_XML_LEN);
    if (hWrite == hDisk) {
        if (sectorWrite < 0) {
            sprintf_s(program_pkt, MAX_XML_LEN,
                "<?xml version=\"1.0\" ?>\n"
                "<data>\n"
                "    <program SECTOR_SIZE_IN_BYTES=\"%d\" num_partition_sectors=\"%d\" physical_partition_number=\"%d\" start_sector=\"NUM_DISK_SECTORS%d\"/>\n"
                "</data>\n",
                DISK_SECTOR_SIZE, (int) sectors, partNum, (int) sectorWrite);
        } else {
            sprintf_s(program_pkt, MAX_XML_LEN,
                "<?xml version=\"1.0\" ?>\n"
                "<data>\n"
                "    <program SECTOR_SIZE_IN_BYTES=\"%d\" num_partition_sectors=\"%d\" physical_partition_number=\"%d\" start_sector=\"%d\"/>\n"
                "</data>\n",
                DISK_SECTOR_SIZE, (int) sectors, partNum, (int) sectorWrite);
        }
    } else {
        sprintf_s(program_pkt, MAX_XML_LEN,
            "<?xml version=\"1.0\" ?>\n"
            "<data>\n"
            "    <read SECTOR_SIZE_IN_BYTES=\"%d\" num_partition_sectors=\"%d\" physical_partition_number=\"%d\" start_sector=\"%d\"/>\n"
            "</data>",
            DISK_SECTOR_SIZE, (int) sectors, partNum, (int) sectorRead);
    }

    LTRACE("Firehose::FastCopy", "正在发送的数据包: \n%s", string_utils::to_hex_view((string) (char*) program_pkt));
    status = sport->Write((BYTE*) program_pkt, strlen(program_pkt));

    while ((status = ReadStatus()) == ERROR_NOT_READY);

    if (status == ERROR_SUCCESS) {
        double st = time_utils::get_time();
        DWORD bytesToRead;
        for (uint32_t tmp_sectors = (uint32_t) sectors; tmp_sectors > 0; tmp_sectors -= (bytesToRead / DISK_SECTOR_SIZE)) {
            bytesToRead = dwMaxPacketSize;
            if (tmp_sectors < dwMaxPacketSize / DISK_SECTOR_SIZE) {
                bytesToRead = tmp_sectors * DISK_SECTOR_SIZE;
            }
            if (hWrite == hDisk) {
                dwBytesRead = 0;
                if (hRead != INVALID_HANDLE_VALUE) {
                    bReadStatus = ReadFile(hRead, m_payload, bytesToRead, &dwBytesRead, NULL);
                }
                if (bReadStatus) {
                    status = sport->Write(m_payload, bytesToRead);
                    if (status != ERROR_SUCCESS) {
                        break;
                    }
                    dwWriteOffset += dwBytesRead;
                } else {
                    status = GetLastError();
                    break;
                }

            } else {
                DWORD offset = 0;
                while (offset < bytesToRead) {
                    dwBytesRead = ReadData(&m_payload[offset], bytesToRead - offset, false);
                    offset += dwBytesRead;
                }
                if (!WriteFile(hWrite, m_payload, bytesToRead, &dwBytesRead, NULL)) {
                    status = GetLastError();
                    break;
                }
            }
            LTRACE("Firehose::FastCopy", "已处理%10d/%10d个扇区, 剩余%10d个 (%.3f%%)",
                (int) (sectors - tmp_sectors + (bytesToRead / DISK_SECTOR_SIZE)),
                (int) sectors,
                (int) (tmp_sectors - (bytesToRead / DISK_SECTOR_SIZE)),
                (sectors - tmp_sectors + (bytesToRead / DISK_SECTOR_SIZE)) * 100.0 / sectors
            );
        }

        LDEBUG("Firehose::FastCopy", "读取完成, 总大小%d字节, 速度%.3fKB/s",
            (int) sectors * DISK_SECTOR_SIZE, 
            (double) ((sectors * DISK_SECTOR_SIZE / 1024.0) / max(time_utils::get_time() - st, 0.0001)));
    }

    if (status == ERROR_SUCCESS) {
        status = ReadStatus();
    }
    if (status != ERROR_SUCCESS) {
        LWARN("Firehose::FastCopy", "快速拷贝过程中出现错误, 状态: %s",
            getErrorDescription(status).c_str());
        return status;
    } else {
        LDEBUG("Firehose::FastCopy", "操作完成, 状态: %s",
            getErrorDescription(status).c_str());
    }
    return status;
}
