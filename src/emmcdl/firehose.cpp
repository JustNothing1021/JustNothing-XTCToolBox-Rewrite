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
#include "tchar.h"
#include "utils/time_utils.h"
#include "utils/logger.h"
#include "utils/string_utils.h"
#include "emmcdl/firehose.h"
#include "emmcdl/xmlparser.h"
#include "emmcdl/utils.h"
#include "emmcdl/partition.h"
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
    // Initialize the serial port
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
        // TODO: 优化XML读取逻辑
        memset(pOutBuf, 0, dwBufSize);
        // First check if there is data available in our buffer
        for (int i = 0; i < 3; i++) {
            for (; m_buffer_len > 0; m_buffer_len--) {
                *pOutBuf++ = *m_buffer_ptr++;
                dwBytesRead++;

                // Check for end of XML
                if (strncmp(((char*) pOutBuf) - 7, "</data>", 7) == 0) {
                    m_buffer_len--;
                    bFoundXML = true;
                    break;
                }
            }

            // If buffer length hits 0 means we didn't find end of XML so read more data
            if (!bFoundXML) {
                // Zero out the buffer to remove any other data and reset pointer
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
        // First copy over any extra data that may be present from previous read
        dwBytesRead = dwBufSize;
        if (m_buffer_len > 0) {
            // Log("Using %d bytes from mbuffer", m_buffer_len);
            LDEBUG("Firehose::ReadData", "缓冲数据剩余量: %d bytes", m_buffer_len);
            // If there is more bytes available then we have space for then only copy out the bytes we need and return
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
                return dwBufSize;
            }

            // Need all the data so copy it all and read any more data that may be present
            memcpy_s(pOutBuf, dwBufSize, m_buffer_ptr, m_buffer_len);
            pOutBuf += m_buffer_len;
            dwBytesRead = dwBufSize - m_buffer_len;
        }

        status = sport->Read(pOutBuf, &dwBytesRead);
        dwBytesRead += m_buffer_len;
        m_buffer_len = 0;
    }

    if (status != ERROR_SUCCESS) {
        LWARN("Firehose::ReadData", "读取设备响应数据包失败: %s", string_utils::wstr2str(getErrorDescription(status)));
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

    // Allocation our global buffers only once when connecting to flash programmer
    if (m_payload == NULL || program_pkt == NULL || m_buffer == NULL) {
        m_payload = (BYTE*) malloc(dwMaxPacketSize);
        program_pkt = (char*) malloc(MAX_XML_LEN);
        m_buffer = (BYTE*) malloc(dwMaxPacketSize);
        if (m_payload == NULL || program_pkt == NULL || m_buffer == NULL) {
            return ERROR_OUTOFMEMORY; // ERROR_HV_INSUFFICIENT_MEMORY
        }
    }
    // Read any pending data from the flash programmer
    memset(m_payload, 0, dwMaxPacketSize);
    dwBytesRead = ReadData((BYTE*) m_payload, dwMaxPacketSize, false);

    // If this is UFS and didn't specify the disk_sector_size then update default to 4096
    if ((_stricmp(cfg->MemoryName, "ufs") == 0) && (DISK_SECTOR_SIZE == 512)) {
        // Log(L"Programming UFS device using SECTOR_SIZE=4096\n");
        LDEBUG("Firehose::ConnectToFlashProg", "使用扇区大小 SECTOR_SIZE=4096 来操作UFS设备");
        DISK_SECTOR_SIZE = 4096;
    } else {
        // Log(L"Programming device using SECTOR_SIZE=%d\n", DISK_SECTOR_SIZE);
        LDEBUG("Firehose::ConnectToFlashProg", "使用扇区大小 SECTOR_SIZE=%d 来操作设备", DISK_SECTOR_SIZE);
    }

    sprintf_s(program_pkt, MAX_XML_LEN,
        "<?xml version=\"1.0\" ?>\n"
        "<data>\n"
        "    <configure MemoryName=\"%s\" ZLPAwareHost=\"%d\" SkipStorageInit=\"%d\" SkipWrite=\"%d\" MaxPayloadSizeToTargetInBytes=\"%d\"/>\n"
        "</data>\n",
        cfg->MemoryName, cfg->ZLPAwareHost, cfg->SkipStorageInit, cfg->SkipWrite, dwMaxPacketSize);
    // Log(program_pkt);
    LTRACE("Firehose::ConnectToFlashProg", "正在发送配置数据包: \n%s", 
        string_utils::to_hex_view((char*) program_pkt, strnlen_s(program_pkt, MAX_XML_LEN)));
    status = sport->Write((BYTE*) program_pkt, strnlen_s(program_pkt, MAX_XML_LEN));
    if (status == ERROR_SUCCESS) {
        // Wait until we get the ACK or NAK back
        for (; retry < MAX_RETRY; retry++) {
            status = ReadStatus();
            if (status == ERROR_SUCCESS) {
                // Received an ACK to configure request we are good to go
                break;
            } else if (status == ERROR_INVALID_DATA) {
                // Received NAK to configure request check reason if it is MaxPayloadSizeToTarget then reduce and retry
                XMLParser xmlparse;
                uint64_t u64MaxSize = 0;
                // Make sure we got a configure response and set our max packet size
                xmlparse.ParseXMLInteger((char*) m_payload, "MaxPayloadSizeToTargetInBytes", &u64MaxSize);

                // If device can't handle a packet this large then change the the largest size they can use and reconfigure
                if ((u64MaxSize > 0) && (u64MaxSize < dwMaxPacketSize)) {
                    LDEBUG("Firehose::ConnectToFlashProg", "由于设备无法处理这么大的数据包(当前为%d, 设备允许的为%d)，将最大数据包大小降低为%d",
                        dwMaxPacketSize, (int) u64MaxSize, (int) u64MaxSize);
                    dwMaxPacketSize = (uint32_t) u64MaxSize;
                    // Log("We are decreasing our max packet size %d\n", dwMaxPacketSize);
                    return ConnectToFlashProg(cfg);
                }
            }
        }

        if (retry == MAX_RETRY) {
            // Log(L"ERROR: No response to configure packet\n");
            LWARN("Firehose::ConnectToFlashProg", "错误: 设备未响应配置数据包 (固定返回 %d)", ERROR_NOT_READY);
            return ERROR_NOT_READY;
        }
    }

    // read out any pending data
    LTRACE("Firehose::ConnectToFlashProg", "读取设备响应数据包...");
    dwBytesRead = ReadData((BYTE*) m_payload, dwMaxPacketSize, false);
    // Log((char*) m_payload);
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
        LWARN("Firehose::DeviceReset", "发送重置命令时出现错误: %s", string_utils::wstr2str(getErrorDescription(status)).c_str());
    return status;
}

int Firehose::ReadStatus(void) {
    // Make sure we read an ACK back
    LTRACE("Firehose::ReadStatus", "检查设备状态");
    while (ReadData(m_payload, MAX_XML_LEN, true) > 0) {
        // Check for ACK packet
        // Log((char*) m_payload);
        // LTRACE("Firehose::ReadStatus", "读取到的数据包:\n%s", 
        //     string_utils::to_hex_view(string((char*) m_payload)));
        if (strstr((char*) m_payload, "ACK") != NULL) {
            LTRACE("Firehose::ReadStatus", "设备状态正常");
            return ERROR_SUCCESS;
        } else if (strstr((char*) m_payload, "NAK") != NULL) {
            // Log("\n---Target returned NAK---\n");
            LWARN("Firehose::ReadStatus", "检查设备状态出现异常，设备返回了NAK");
            return ERROR_INVALID_DATA;
        }
    }
    LWARN("Firehose::ReadStatus", "检查设备状态出现异常，设备未响应 (固定返回 %d)", ERROR_NOT_READY);
    return ERROR_NOT_READY;
}

int Firehose::ProgramPatchEntry(PartitionEntry pe, TCHAR* key) {
    UNREFERENCED_PARAMETER(pe);
    TCHAR tmp_key[MAX_STRING_LEN];
    size_t bytesOut;
    int status = ERROR_SUCCESS;

    // Make sure we get a valid parameter passed in
    if (key == NULL)
        return ERROR_INVALID_PARAMETER;
    wcscpy_s(tmp_key, key);

    memset(program_pkt, 0, MAX_XML_LEN);
    sprintf_s(program_pkt, MAX_XML_LEN, "<?xml version=\"1.0\" ?><data>");
    StringReplace(tmp_key, L".", L"");
    StringReplace(tmp_key, L".", L"");
    wcstombs_s(&bytesOut, &program_pkt[strlen(program_pkt)], 2000, tmp_key, MAX_XML_LEN);
    strcat_s(program_pkt, MAX_XML_LEN, "></data>\n");
    LTRACE("Firehose::ProgramPatchEntry", "正在发送的数据包: \n%s", 
        string_utils::to_hex_view(string((char*) program_pkt)));
    status = sport->Write((BYTE*) program_pkt, strlen(program_pkt));
    if (status != ERROR_SUCCESS) {
        LWARN("Firehose::ProgramPatchEntry", "发送数据包时出现错误: %s", string_utils::wstr2str(getErrorDescription(status)).c_str());
        return status;
    }

    int status2 = ReadStatus();
    if (status2 != ERROR_SUCCESS) {
        LWARN("Firehose::ProgramPatchEntry", "检查设备状态出现异常: %s", string_utils::wstr2str(getErrorDescription(status2)).c_str());
        return status2;
    }
    return status;
}

int Firehose::WriteData(BYTE* writeBuffer, int64_t writeOffset, DWORD writeBytes, DWORD* bytesWritten, uint8_t partNum) {
    DWORD dwBytesRead;
    int status = ERROR_SUCCESS;

    // If we are provided with a buffer read the data directly into there otherwise read into our internal buffer
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
    } else { // If start sector is negative write to back of disk
        sprintf_s(program_pkt, MAX_XML_LEN,
            "<?xml version=\"1.0\" ?>\n"
            "<data>\n"
            "<program SECTOR_SIZE_IN_BYTES=\"%d\" num_partition_sectors=\"%d\" physical_partition_number=\"%d\" start_sector=\"NUM_DISK_SECTORS%d\"/>\n"
            "</data>\n",
            DISK_SECTOR_SIZE, (int) writeBytes / DISK_SECTOR_SIZE, partNum, (int) (writeOffset / DISK_SECTOR_SIZE));
    }

    LTRACE("Firehose::WriteData", "正在发送的数据包: \n%s", string_utils::to_hex_view(string((char*) program_pkt)));
    status = sport->Write((BYTE*) program_pkt, strlen(program_pkt));

    // This should have log information from device
    // Get the response after read is done
    // uint64_t ticks = GetTickCount64();
    double st = time_utils::get_time();

    if (ReadStatus() != ERROR_SUCCESS)
        goto WriteSectorsExit;

    // uint64_t dwBufSize = writeBytes;

    // loop through and write the data
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
        // wprintf(L"Sectors remaining %d\r", (int) (writeBytes - i));
        LTRACE("Firehose::WriteData", "已写入%10d/%10dbytes, 剩余%10dbytes (%.3f%%)",
            (int) i, (int) writeBytes,
            (int) (writeBytes - i),
            (double) i / writeBytes * 100.0
        );
    }

    // wprintf(L"\nDownloaded raw image size %d at speed %d KB/s\n", (int) writeBytes, (int) (((writeBytes / 1024) * 1000) / (GetTickCount64() - ticks + 1)));

    LDEBUG("Firehose::WriteData", "写入完成，总大小%d字节, 速度%.3fKB/s",
        (int) writeBytes, (double) ((writeBytes / 1024.0) / (max(time_utils::get_time() - st, 0.0001))));

    // Get the response after read is done
    LDEBUG("Firehose::WriteData", "等待设备响应");
    status = ReadStatus();
    LDEBUG("Firehose::WriteData", "设备响应完成, 状态: %s",
        string_utils::wstr2str(getErrorDescription(status)).c_str());

    // Read and display any other log packets we may have

WriteSectorsExit:
    return status;
}

int Firehose::ReadData(BYTE* readBuffer, int64_t readOffset, DWORD readBytes, DWORD* bytesRead, uint8_t partNum) {
    LTRACE("Firehose::ReadData", "正在读取数据, 起始偏移量: %llu, 预计读取大小: %llu, 分区号: %d, 数据和实际长度保存至%p和%p", 
        readOffset, readBytes, partNum, (void*) readBuffer, (void*) bytesRead);
    DWORD dwBytesRead;
    int status = ERROR_SUCCESS;

    // If we are provided with a buffer read the data directly into there otherwise read into our internal buffer
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
    } else { // If start sector is negative read from back of disk
        sprintf_s(program_pkt, MAX_XML_LEN,
            "<?xml version=\"1.0\" ?>\n"
            "<data>\n"
            "<read SECTOR_SIZE_IN_BYTES=\"%d\" num_partition_sectors=\"%d\" physical_partition_number=\"%d\" start_sector=\"NUM_DISK_SECTORS%d\"/>\n"
            "</data>\n",
            DISK_SECTOR_SIZE, (int) readBytes / DISK_SECTOR_SIZE, partNum, (int) (readOffset / DISK_SECTOR_SIZE));
    }

    LTRACE("Firehose::ReadData", "正在发送的数据包: \n%s", string_utils::to_hex_view(string((char*) program_pkt)));

    status = sport->Write((BYTE*) program_pkt, strlen(program_pkt));

    // Wait until device returns with ACK or NAK
    while ((status = ReadStatus()) == ERROR_NOT_READY);
    // uint64_t ticks = GetTickCount64();
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

        // Now either write the data to the buffer or handle given
        readBuffer += bytesToRead;
        *bytesRead += bytesToRead;
        // wprintf(L"Sectors remaining %d\r", (int) tmp_sectors);
        LTRACE("Firehose::ReadData", "已读取%10d/%10dbytes, 剩余%10dbytes (%.3f%%)",
            (int) (*bytesRead), (int) readBytes,
            (int) (tmp_sectors * DISK_SECTOR_SIZE - bytesToRead),
            (double) (*bytesRead) / readBytes * 100.0
        );
    }

    // wprintf(L"\nDownloaded raw image at speed %d KB/s\n", (int) (readBytes / (GetTickCount64() - ticks + 1)));
    LDEBUG("Firehose::ReadData", "读取完成, 总大小%d字节, 速度%.3fKB/s",
        (int) readBytes, (double) ((readBytes / 1024.0) / max(time_utils::get_time() - st, 0.0001)));

    // Get the response after read is done first response should be finished command
    LTRACE("Firehose::ReadData", "等待设备响应");
    status = ReadStatus();
    LTRACE("Firehose::ReadData", "设备响应完成, 状态: %s",
        string_utils::wstr2str(getErrorDescription(status)).c_str());

ReadSectorsExit:
    if (status != ERROR_SUCCESS) {
        LWARN("Firehose::ReadData", "读取数据遇到错误, 状态: %s",
            string_utils::wstr2str(getErrorDescription(status)).c_str());
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


    // Read response
    status = ReadStatus();

    // Read and display any other log packets we may have
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
    // Log((char*) program_pkt);


    // Read response
    LTRACE("Firehose::SetActivePartition", "等待设备响应");
    status = ReadStatus();
    LDEBUG("Firehose::SetActivePartition", "设备响应完成, 状态: %s",
        string_utils::wstr2str(getErrorDescription(status)).c_str());

    // Read and display any other log packets we may have
    if (ReadData(m_payload, dwMaxPacketSize, false) > 0)
        LTRACE("Firehose::SetActivePartition", "设备响应: \n%s", (char*) m_payload);

    return status;
}

int Firehose::ProgramRawCommand(TCHAR* key) {
    DWORD dwBytesRead;
    size_t bytesOut;
    int status = ERROR_SUCCESS;
    memset(program_pkt, 0, MAX_XML_LEN);
    sprintf_s(program_pkt, MAX_XML_LEN, "<?xml version=\"1.0\" ?><data>");
    wcstombs_s(&bytesOut, &program_pkt[strlen(program_pkt)], MAX_STRING_LEN, key, MAX_XML_LEN);
    strcat_s(program_pkt, MAX_XML_LEN, "></data>\n");
    LTRACE("Firehose::ProgramRawCommand", "正在发送的数据包: \n%s", string_utils::to_hex_view((string) (char*) program_pkt));
    status = sport->Write((BYTE*) program_pkt, strlen(program_pkt));
    // Log("Programming RAW command: %s\n", (char*) program_pkt);

    // Read and log any response to command we sent

    dwBytesRead = ReadData(m_payload, dwMaxPacketSize, false);
    // Log("%s\n", (char*) m_payload);
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

    // If we are provided with a buffer read the data directly into there otherwise read into our internal buffer
    if (hWrite == NULL) {
        return ERROR_INVALID_PARAMETER;
    }

    if (hRead == INVALID_HANDLE_VALUE) {
        // wprintf(L"hRead = INVALID_HANDLE_VALUE, zeroing input buffer\n");
        LDEBUG("Firehose::FastCopy", "读取句柄无效, 将输入缓冲区清零");
        memset(m_payload, 0, dwMaxPacketSize);
        dwBytesRead = dwMaxPacketSize;
    } else {
        // Move file pointer to the give location if value specified is > 0
        if (sectorRead > 0) {
            LONG sectorReadHigh = dwReadOffset >> 32;
            status = SetFilePointer(hRead, (LONG) dwReadOffset, &sectorReadHigh, FILE_BEGIN);
            if (status == INVALID_SET_FILE_POINTER) {
                status = GetLastError();
                // wprintf(L"Failed to set offset 0x%llx status: %d\n", sectorRead, status);
                LERROR("Firehose::FastCopy", "设置读取偏移0x%llx失败, 状态: %s",
                    sectorRead, string_utils::wstr2str(getErrorDescription(status)).c_str());
                return status;
            }
        }
    }

    // Determine if we are writing to firehose or reading from firehose
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

    // Write out the command and wait for ACK/NAK coming back
    LTRACE("Firehose::FastCopy", "正在发送的数据包: \n%s", string_utils::to_hex_view((string) (char*) program_pkt));
    status = sport->Write((BYTE*) program_pkt, strlen(program_pkt));
    // Log((char*) program_pkt);


    // Wait until device returns with ACK or NAK
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
                // Writing to disk and reading from file...
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
                    // If there is partial data read out then write out next chunk to finish this up
                    status = GetLastError();
                    break;
                }

            } // Else this is a read command so read data from device in dwMaxPacketSize chunks
            else {
                DWORD offset = 0;
                while (offset < bytesToRead) {
                    dwBytesRead = ReadData(&m_payload[offset], bytesToRead - offset, false);
                    offset += dwBytesRead;
                }
                // Now either write the data to the buffer or handle given
                if (!WriteFile(hWrite, m_payload, bytesToRead, &dwBytesRead, NULL)) {
                    // Failed to write out the data
                    status = GetLastError();
                    break;
                }
            }
            // wprintf(L"Sectors remaining %d\r", (int) tmp_sectors);
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

    // Get the response after raw transfer is completed
    if (status == ERROR_SUCCESS) {
        status = ReadStatus();
    }
    if (status != ERROR_SUCCESS) {
        LWARN("Firehose::FastCopy", "快速拷贝过程中出现错误, 状态: %s",
            string_utils::wstr2str(getErrorDescription(status)).c_str());
        return status;
    } else {
        LDEBUG("Firehose::FastCopy", "操作完成, 状态: %s",
            string_utils::wstr2str(getErrorDescription(status)).c_str());
    }
    return status;
}
