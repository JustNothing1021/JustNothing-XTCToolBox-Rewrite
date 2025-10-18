#pragma once

#include <stdio.h>
#include <Windows.h>
#include <stdlib.h>
#include <tchar.h>
#include <string>
#include <vector>
#include <stdint.h>
#include <algorithm>
#include "emmcdl/crc.h"
#include "datatypes/bytearray.h"



#define  ASYNC_HDLC_FLAG      0x7e 
#define  ASYNC_HDLC_ESC       0x7d
#define  ASYNC_HDLC_ESC_MASK  0x20
#define  MAX_PACKET_SIZE      0x20000

// Serial port.
// 串口类。
class SerialPort {
public:
    // Constructor.
    //
    // 构造函数。
    SerialPort();

    // Destructor. Closes the port and frees the buffer memory.
    //
    // 析构函数。会释放掉打开的串口并释放缓冲区内存。
    ~SerialPort();
    /**
     * @brief
     * Open serial port.
     *
     * 打开串口。
     * @param port [in] Serial port number.  串口号。
     * @return
     * Status.
     *
     * 错误代码。
     */

    int Open(int port);
    /**
     * @brief
     * Get port status.
     *
     * 获取串口状态。
     * @param comStat [out] COMSTAT structure to fill with port status. 用于存储的串口状态结构体的地址。
     * @param errors  [out] DWORD to fill with error status.            用于存储的错误代码的变量的地址。
     * @return
     * Status.
     *
     * 错误代码。
     */

    bool GetPortStatus(COMSTAT& comStat, DWORD& errors);
    /**
     * @brief 
     * Enable binary log, which records every IO opreations.
     * 
     * 启用串口日志（会记录所有IO操作）。
     * @param enable         [in] Enable or disable.  是否启用。
     * @param dirName        [in] Directory to store log files.  存储日志文件的目录。
     * @param dwLogSizeLimit [in] Limit of logging. Opreations bigger than this will not be fully shown.
     *                              日志记录的大小限制。超过这个大小的IO操作将不会完全显示。
     * @param dwFileSizeLimit    [in] Limit of each record. Opreations bigger than this will not be recorded. 
     *                              每个日志文件的大小限制。超过这个大小的IO操作将不会被记录。
     * @return ERROR_SUCCESS
     */

    int EnableBinaryLog(bool enable = true, 
                        std::string dirName = "log/ports", 
                        DWORD dwLogSizeLimit = 0x1000, 
                        DWORD dwFileSizeLimit = 0x2000000);
    /**
     * @brief
     * Close serial port.
     *
     * 关闭串口。
     * @return
     * Status.
     *
     * 错误代码。
     */
    int Close();

    /**
     * @brief
     * Write data to serial port.
     *
     * 向串口写入数据。
     * @param data         [in] Data to write.                   要写入的数据。
     * @param length       [in] Maximum length of data to write. 要写入的数据的最大长度。
     * @param bytesWritten [out] Number of bytes actually written.   实际写入的字节数。
     * @return
     * Status.
     *
     * 错误代码。
     */
    int Write(const ByteArray& data, DWORD length = -1, DWORD* bytesWritten = nullptr);


    /**
     * @brief
     * Write data to serial port.
     *
     * 向串口写入数据。
     * @param data       [in] Data to write.                   要写入的数据。
     * @param length     [in] Maximum length of data to write. 要写入的数据的最大长度。
     * @return
     * Status.
     *
     * 错误代码。
     */
    int Write(const BYTE* data, DWORD length);

    /**
     * @brief 
     * Read data from serial port. 
     * 
     * 从串口读取数据。
     * @param data       [out] Buffer to read data into.      用于存储读取到的数据的缓冲区的地址。
     * @param max_length [in] Maximum length of data to read. 要读取的数据的最大长度。
     * @param bytesRead  [out] Number of bytes actually read.   实际读取到的字节数。
     * @return 
     * Status.
     * 
     * 错误代码。
     */
    int Read(ByteArray& data, DWORD max_length = -1, DWORD* bytesRead = nullptr);


    /**
     * @brief
     * Read data from serial port.
     * 
     * 从串口读取数据。
     * @param data       [out] Buffer to read data into.      用于存储读取到的数据的缓冲区的地址。
     * @param max_length [in/out] Size of the buffer, as the limit of data to read.
     *                              On return this will contain the number of bytes actually read.
     *                           缓冲区的大小，也就是要读取的数据的最大长度。返回时，这个值将变为实际读取到的字节数。
     */
    int Read(BYTE* data, DWORD* length);

    /**
     * @brief 
     * Flush serial port.    
     * 
     * 清空串口缓冲区。
     * @return 
     * Status. 
     * 
     * 错误代码。
     */
    int Flush();

    /**
     * @brief 
     * Send a packet to the device and wait for response. 
     * 
     * 向设备发送数据包并等待响应。
     * @param out_buf    [in]     Buffer containing data to send.    包含要发送的数据的缓冲区。
     * @param in_buf     [out]    Buffer to read response into.      用于存储响应数据的缓冲区。
     * @return 
     * Status.
     * 
     * 错误代码。
     */
    int SendSync(const ByteArray& out_buf, ByteArray& in_buf);
    int SendSync(BYTE *out_buf, int out_length, BYTE *in_buf, int *in_length);

    /**
     * @brief 
     * Set the timeout for read/write operations. 
     * 
     * 设置读写操作的超时时间。
     * @param miliseconds [in] Timeout in milliseconds. 超时时间，单位为毫秒。
     * @return ERROR_SUCCESS
     */
    int SetTimeout(int miliseconds);

private:

    /**
     * @brief 
     * HDLC encode a packet.  
     * 
     * HDLC编码数据包。
     * @param in_buf     [in]     Buffer containing data to send.    包含要发送的数据的缓冲区。
     * @param out_buf    [out]    Buffer to read response into.      用于存储响应数据的缓冲区。
     * @return ERROR_SUCCESS
     */
    int HDLCEncodePacket(const ByteArray& in_buf, ByteArray& out_buf);
    int HDLCEncodePacket(BYTE* in_buf, int in_length, BYTE* out_buf, int* out_length);
    
    /**
     * @brief 
     * HDLC decode a packet. 
     * 
     * HDLC解码数据包。
     * @param in_buf     [in]     Buffer containing data to decode. 包含要解码的数据的缓冲区。
     * @param out_buf    [out] Buffer to read decoded data into.  用于存储解码数据的缓冲区。
     * @return ERROR_SUCCESS
     */
    int HDLCDecodePacket(const ByteArray& in_buf, ByteArray& out_buf);
    int HDLCDecodePacket(BYTE* in_buf, int in_length, BYTE* out_buf, int* out_length);

    void WriteBinaryLog(std::string logTitle, std::string fileName, const ByteArray& data);

    int portNum;            // Serial port number. 串口号。
    HANDLE hPort;        // Serial port handle. 串口句柄。
    BYTE* HDLCBuf;       // Buffer for HDLC encoding/decoding. HDLC编码/解码缓冲区。
    int timeout_ms;      // Timeout for read/write operations. 读写操作的超时时间。
    ByteArray baHDLCBuf; // ByteArray version of HDLCBuf. HDLCBuf的ByteArray版本。
    bool bBinaryLog;     // Whether binary log is enabled. 是否启用了串口日志。
    std::string sLogDirName; // Directory to store log files. 存储日志文件的目录。
    DWORD dwLogRecordThershold; // Threshold of each record in binary log. 串口日志中每个日志文件的大小阈值。
    DWORD dwBinaryLogSizeLimit; // Limit of each record in binary log. 串口日志中每个日志文件的大小限制。
};
