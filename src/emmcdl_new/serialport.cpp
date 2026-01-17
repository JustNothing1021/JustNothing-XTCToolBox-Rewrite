
#include "emmcdl_new/serialport.h"
#include "emmcdl_new/utils.h"
#include "utils/logger.h"
#include "utils/time_utils.h"

using namespace std;

SerialPort::SerialPort() {
    hPort = INVALID_HANDLE_VALUE;
    timeout_ms = 1000;  // 1 second default timeout for packets to send/rcv
    HDLCBuf = (BYTE*) malloc(MAX_PACKET_SIZE);
    baHDLCBuf.resize(MAX_PACKET_SIZE);
}


SerialPort::~SerialPort() {
    if (hPort != INVALID_HANDLE_VALUE) CloseHandle(hPort);
    if (HDLCBuf) free(HDLCBuf);
}


int SerialPort::EnableBinaryLog(bool enable, string path, DWORD sz1, DWORD sz2) {
    dwBinaryLogSizeLimit = sz2;
    dwLogRecordThershold = sz1;
    bBinaryLog = enable;
    sLogDirName = path;
    return ERROR_SUCCESS;
}


// TODO: 在调用之前就判断文件大小需不需要记录以节约性能
//       可以的话可以再加个重载版本（传BYTE*和长度）
void SerialPort::WriteBinaryLog(std::string logTitle, std::string fileName, const ByteArray& data) {
    if (!bBinaryLog) return;
    // 如果不存在文件夹就创建一个
    if (!std::filesystem::exists(sLogDirName)) {
        std::filesystem::create_directories(sLogDirName);
    }
    LTRACE("SerialPort::WriteBinaryLog", "COM%d的串口日志: %s", portNum, logTitle.c_str());
    DWORD dSize = data.size();
    string content;
    if (dSize > dwLogRecordThershold) {
        content = "  <数据过长，" + to_string(dSize) + "bytes>";
        return;
    } else {
        content = data.empty() ?  "  <无数据>" : data.to_hex_view();
    }
    LTRACE("SerialPort::WriteBinaryLog", content);
    if (dSize < dwBinaryLogSizeLimit) {
        FILE* fp = fopen((sLogDirName + "/" + fileName).c_str(), "a");
        if (fp) {
            fwrite(data.c_str(), 1, dSize, fp);
            // 不要用fprintf，会把'\0'之后的所有内容都忽略掉
            fclose(fp);
        } else {
            LWARN("SerialPort::WriteBinaryLog", "无法打开日志文件: %s", fileName.c_str());
        }
    }

}

int SerialPort::Open(int port) {
    TCHAR tPath[32];
    if (port < 0) {
        LERROR("SerialPort::Open", "无效的端口号：%d，返回ERROR_INVALID_PARAMETER", port);
        return ERROR_INVALID_PARAMETER;
    }
    portNum = port;
    swprintf_s(tPath, 32, L"\\\\.\\COM%d", port);
    // Open handle to serial port and set proper port settings
    LDEBUG("SerialPort::Open", "打开串口COM%d", port);
    hPort = CreateFileW(tPath,
        (GENERIC_READ | GENERIC_WRITE),
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        0,
        NULL);
    if (hPort != INVALID_HANDLE_VALUE) {
        SetTimeout(timeout_ms);
        DCB dcb = {0};
        // dcb.DCBlength = sizeof(dcb);
        if (GetCommState(hPort, &dcb)) {
            uint32_t baudRate = dcb.BaudRate;
            LDEBUG("SerialPort::Open", "COM%d打开成功，波特率：%d", port, baudRate);
        }
        return ERROR_SUCCESS;
    }
    int status = GetLastError();
    LERROR("SerialPort::Open", "COM%d打开失败，状态：%s", 
        port, getErrorDescription(status).c_str());
    return status;
}


bool SerialPort::GetPortStatus(COMSTAT& comStat, DWORD& errors) {
    return ClearCommError(hPort, &errors, &comStat);
}


int SerialPort::Close() {
    if (hPort != INVALID_HANDLE_VALUE) {
        CloseHandle(hPort);
    }
    hPort = INVALID_HANDLE_VALUE;
    return ERROR_SUCCESS;
}



int SerialPort::Write(const ByteArray &data, DWORD max_length, DWORD* bytesWritten) {
    int status = ERROR_SUCCESS;
    if (!WriteFile(hPort,
                    data.c_str(), 
                    min((DWORD) data.size(), max_length),   
                    // 一定要敲min，不然会访问到内存外面去，然后报998（ERROR_NOACCESS）
                    bytesWritten, NULL)) {
        // Write not complete
        status = GetLastError();
        LWARN("SerialPort::Write", "向串口COM%d写入数据失败：%s", 
            portNum,
            getErrorDescription(status).c_str());
    }
    WriteBinaryLog("HOST to TARGET =====>", 
        time_utils::get_formatted_time_with_frac(
            time_utils::get_time(), 
            fmt::format("COM{}_%Y%m%d_%H%M%S%f__FROM_HOST_ERRNO_{}.bin", portNum, status)),
        data);
    return status;
}

int SerialPort::Read(ByteArray &data, DWORD max_length, DWORD* bytesRead) {
    COMSTAT comStat;
    DWORD errors;
    GetPortStatus(comStat, errors);
    size_t bytesToRead = std::min(comStat.cbInQue, max_length);
    data.clear();
    data.resize(bytesToRead);
    int status = ERROR_SUCCESS;
    // Read data in from serial port
    if (!ReadFile(hPort, data.data(), bytesToRead, bytesRead, NULL)) {
        status = GetLastError();
        LWARN("SerialPort::Read", "从串口COM%d读取数据失败：%s", getErrorDescription(status).c_str());
    }
    WriteBinaryLog("TARGET to HOST  <=====", 
        time_utils::get_formatted_time_with_frac(
            time_utils::get_time(), 
            fmt::format("COM{}_%Y%m%d_%H%M%S%f__FROM_TARGET_ERRNO_{}.bin", portNum, status)),
        data);
    return status;
}


int SerialPort::Write(const BYTE* data, DWORD length) {
    int status = ERROR_SUCCESS;
    if (!WriteFile(hPort, data, length, &length, NULL)) {
        status = GetLastError();
        LWARN("SerialPort::Write", "向串口COM%d写入数据失败：%s", 
            portNum,
            getErrorDescription(status).c_str());
    }
    WriteBinaryLog("HOST to TARGET  =====>", 
        time_utils::get_formatted_time_with_frac(
            time_utils::get_time(), 
            fmt::format("COM{}_%Y%m%d_%H%M%S%f__FROM_HOST_ERRNO_{}.bin", portNum, status)),
        ByteArray(data, length));
    return status;
}



int SerialPort::Read(BYTE* data, DWORD* length) {
    int status = ERROR_SUCCESS;

    // Read data in from serial port
    if (!ReadFile(hPort, data, *length, length, NULL)) {
        status = GetLastError();
        LWARN("SerialPort::Read", "从串口COM%d读取数据失败：%s", 
            portNum,
            getErrorDescription(status).c_str()
        );
    }
    WriteBinaryLog("TARGET to HOST  <=====", 
        time_utils::get_formatted_time_with_frac(
            time_utils::get_time(), 
            fmt::format("COM{}_%Y%m%d_%H%M%S%f__FROM_TARGET_ERRNO_{}.bin", portNum, status)),
        ByteArray(data, *length));
    return status;
}




int SerialPort::Flush() {
    ByteArray tmp;
    // Set timeout to 1ms to just flush any pending data then change back to default
    SetTimeout(1);
    Read(tmp, 1024);
    SetTimeout(timeout_ms);
    PurgeComm(hPort, PURGE_RXABORT | PURGE_TXABORT | PURGE_RXCLEAR | PURGE_TXCLEAR);
    return ERROR_SUCCESS;
}



int SerialPort::SendSync(const ByteArray &out_buf, ByteArray &in_buf) {
    DWORD status = ERROR_SUCCESS;
    DWORD bytesOut = 0;
    DWORD bytesIn = 0;
    if (hPort == INVALID_HANDLE_VALUE) {
        return ERROR_INVALID_HANDLE;
    }
    bytesOut = MAX_PACKET_SIZE;
    status = HDLCEncodePacket(out_buf, baHDLCBuf);
    status = Write(baHDLCBuf, bytesOut);
    if (status != ERROR_SUCCESS) return status;
    baHDLCBuf.clear();
    baHDLCBuf.resize(MAX_PACKET_SIZE);
    size_t bufferSize = baHDLCBuf.size();
    for (size_t i = 0; ; i++) {
        bytesIn = 1;
        // status = Read(&HDLCBuf[i], &bytesIn);
        ByteArray tmp;
        status = Read(tmp, bytesIn, &bytesIn);
        baHDLCBuf.replace(i, bytesIn, tmp);
        if (status != ERROR_SUCCESS) return status;
        if (bytesIn == 0) break;
        if (HDLCBuf[i] == 0x7E) {
            if (i > 1) {
                bytesIn = i + 1;
                break;
            } else {
                i = 0;
            }
        }
        if (i == bufferSize) {  // Allocate more space
            baHDLCBuf.resize(bufferSize + MAX_PACKET_SIZE);
            bufferSize = baHDLCBuf.size();
        }
    }
    baHDLCBuf.resize(bytesIn);
    status = HDLCDecodePacket(baHDLCBuf, in_buf);
    baHDLCBuf.clear();
    return status;
}

int SerialPort::SendSync(BYTE* out_buf, int out_length, BYTE* in_buf, int* in_length) {
    DWORD status = ERROR_SUCCESS;
    DWORD bytesOut = 0;
    DWORD bytesIn = 0;

    // As long as hPort is valid write the data to the serial port and wait for response for timeout
    if (hPort == INVALID_HANDLE_VALUE) {
        return ERROR_INVALID_HANDLE;
    }

    // Do HDLC encoding then send out packet
    bytesOut = MAX_PACKET_SIZE;
    status = HDLCEncodePacket(out_buf, out_length, HDLCBuf, (int*) &bytesOut);

    // We know we have a good handle now so write out data and wait for response
    status = Write(HDLCBuf, bytesOut);
    if (status != ERROR_SUCCESS) return status;

    // Keep looping through until we have received 0x7e with size > 1
    for (int i = 0;i < MAX_PACKET_SIZE;i++) {
        // Now read response and and wait for up to timeout
        bytesIn = 1;
        status = Read(&HDLCBuf[i], &bytesIn);
        if (status != ERROR_SUCCESS) return status;
        if (bytesIn == 0) break;
        if (HDLCBuf[i] == 0x7E) {
            if (i > 1) {
                bytesIn = i + 1;
                break;
            } else {
                // If packet starts with multiple 0x7E ignore
                i = 0;
            }
        }

    }

    // Remove HDLC encoding and return the packet
    status = HDLCDecodePacket(HDLCBuf, bytesIn, in_buf, in_length);

    return status;
}


int SerialPort::SetTimeout(int miliseconds) {
    COMMTIMEOUTS commTimeout;
    commTimeout.ReadTotalTimeoutConstant = miliseconds;
    commTimeout.ReadIntervalTimeout = MAXDWORD;
    commTimeout.ReadTotalTimeoutMultiplier = MAXDWORD;
    commTimeout.WriteTotalTimeoutConstant = miliseconds;
    commTimeout.WriteTotalTimeoutMultiplier = MAXDWORD;
    SetCommTimeouts(hPort, &commTimeout);

    timeout_ms = miliseconds;
    return ERROR_SUCCESS;
}



int SerialPort::HDLCEncodePacket(BYTE* in_buf, int in_length, BYTE* out_buf, int* out_length) {
    BYTE* outPtr = out_buf;
    unsigned short crc = (unsigned short) CalcCRC16(in_buf, in_length);

    

    // Encoded packets start and end with 0x7E
    *outPtr++ = ASYNC_HDLC_FLAG;
    for (int i = 0; i < in_length + 2; i++) {
        // Read last two bytes of data from crc value
        if (i == in_length) {
            in_buf = (BYTE*) &crc;
        }

        if (*in_buf == ASYNC_HDLC_FLAG ||
            *in_buf == ASYNC_HDLC_ESC) {
            *outPtr++ = ASYNC_HDLC_ESC;
            *outPtr++ = *in_buf++ ^ ASYNC_HDLC_ESC_MASK;
        } else {
            *outPtr++ = *in_buf++;
        }
    }
    *outPtr++ = ASYNC_HDLC_FLAG;

    // Update length of packet
    *out_length = outPtr - out_buf;
    return ERROR_SUCCESS;
}




int SerialPort::HDLCEncodePacket(const ByteArray &in_buf, ByteArray &out_buf) {
    out_buf.clear();
    size_t realSize = 0;
    out_buf.resize(in_buf.size() * 2 + 4); // Worst case
    int status = HDLCEncodePacket((BYTE*) in_buf.data(), (int) in_buf.size(), (BYTE*) out_buf.data(), (int*) &realSize);
    out_buf.resize(realSize);
    return status;
}


int SerialPort::HDLCDecodePacket(BYTE* in_buf, int in_length, BYTE* out_buf, int* out_length) {
    BYTE* outPtr = out_buf;
    // make sure our output buffer is large enough
    if (*out_length < in_length) {
        return ERROR_NOT_ENOUGH_MEMORY;
    }

    for (int i = 0; i < in_length; i++) {
        switch (*in_buf) {
            case ASYNC_HDLC_FLAG:
                // Do nothing it is an escape flag
                in_buf++;
                break;
            case ASYNC_HDLC_ESC:
                in_buf++;
                *outPtr++ = *in_buf++ ^ ASYNC_HDLC_ESC_MASK;
                break;
            default:
                *outPtr++ = *in_buf++;
        }
    }

    *out_length = outPtr - out_buf;
    // Should do CRC check here but for now we are using USB so assume good

    return ERROR_SUCCESS;
}


int SerialPort::HDLCDecodePacket(const ByteArray &in_buf, ByteArray &out_buf) {
    out_buf.clear();
    size_t realSize = 0;
    out_buf.resize(in_buf.size());
    int status = HDLCDecodePacket((BYTE*) in_buf.data(), (int) in_buf.size(), (BYTE*) out_buf.data(), (int*) &realSize);
    out_buf.resize(realSize);
    return status;
}