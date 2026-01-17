/*****************************************************************************
 * sparse.h
 *
 * This file implements the sparse image format handling for Android flashing
 * 本文件实现了用于 Android 刷机的稀疏镜像格式处理
 *
 * Sparse image format is used in Android to efficiently store and flash
 * large images with empty blocks, reducing file size and flash time.
 * 稀疏镜像格式用于 Android 中高效存储和刷写包含空块的大镜像，
 * 减少文件大小和刷机时间。
 *
 *****************************************************************************/

#pragma once

#include <string>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>
#include "emmcdl_new/protocol.h"
#include "datatypes/bytearray.h"

// Sparse image magic number
// 稀疏镜像魔数
#define SPARSE_MAGIC      0xED26FF3A

// Sparse image chunk types
// 稀疏镜像块类型
#define SPARSE_RAW_CHUNK  0xCAC1   // Raw data chunk / 原始数据块
#define SPARSE_FILL_CHUNK 0xCAC2   // Fill chunk with repeated data / 填充块（重复数据）
#define SPARSE_DONT_CARE  0xCAC3   // Don't care chunk (skip) / 忽略块（跳过）

/**
 * @struct SPARSE_HEADER
 * @brief Sparse image header structure.
 *        稀疏镜像头结构体。
 *
 * This structure contains the header information for a sparse image file.
 * The sparse image format is used to efficiently represent images with
 * large amounts of empty or repeated data.
 * 此结构体包含稀疏镜像文件的头部信息。稀疏镜像格式用于高效表示
 * 包含大量空白或重复数据的镜像。
 */
typedef struct _SPARSE_HEADER {
    uint32_t dwMagic;           // 0xed26ff3a - Magic number / 魔数
    uint16_t wVerMajor;         // 0x1 - Major version, reject version with higher major version / 主版本号，拒绝更高主版本
    uint16_t wVerMinor;         // 0x0 - Minor version, accept version with higher minor version / 次版本号，接受更高次版本
    uint16_t wSparseHeaderSize; // 28 bytes for first revision of the file format / 文件格式第一版的稀疏头大小（28字节）
    uint16_t wChunkHeaderSize;  // 12 bytes for first revision of the file format / 文件格式第一版的块头大小（12字节）
    uint32_t dwBlockSize;       // 4096 block size in bytes must be multiple of 4 / 块大小（字节），必须是4的倍数
    uint32_t dwTotalBlocks;     // Total blocks in the non-sparse output file / 非稀疏输出文件中的总块数
    uint32_t dwTotalChunks;     // Total chunks in the sparse input image / 稀疏输入镜像中的总块数
    uint32_t dwImageChecksum;   // CRC32 checksum of the original data counting do not care 802.3 polynomial / 原始数据的 CRC32 校验和（802.3 多项式）
} SPARSE_HEADER;

/**
 * @struct CHUNK_HEADER
 * @brief Sparse image chunk header structure.
 *        稀疏镜像块头结构体。
 *
 * This structure contains the header information for each chunk in a sparse image.
 * Each chunk can be raw data, fill data, or don't care (skip).
 * 此结构体包含稀疏镜像中每个块的头部信息。每个块可以是原始数据、
 * 填充数据或忽略（跳过）。
 */
typedef struct _CHUNK_HEADER {
    uint16_t wChunkType;        // 0xCAC1 -> raw; 0xCAC2 -> fill; 0xCAC3 -> don't care / 块类型：0xCAC1=原始，0xCAC2=填充，0xCAC3=忽略
    uint16_t wReserved;         // Reserved should be all 0 / 保留字段，应该全为0
    uint32_t dwChunkSize;       // Number of blocks this chunk takes in output image / 此块在输出镜像中占用的块数
    uint32_t dwTotalSize;       // Number of bytes in input file including chunk header round up to next block size / 输入文件中的字节数（包括块头，向上取整到下一个块大小）
} CHUNK_HEADER;

/**
 * @class SparseImage
 * @brief Sparse image handler for Android flashing.
 *        Android 刷机的稀疏镜像处理器。
 *
 * This class provides functionality to parse and program sparse images
 * to devices using the Protocol interface. Sparse images are used in
 * Android to efficiently flash large system images.
 * 本类提供解析和编程稀疏镜像到设备的功能，使用 Protocol 接口。
 * 稀疏镜像用于 Android 中高效刷写大型系统镜像。
 */
class SparseImage {
public:
    /**
     * @brief Constructor.
     *        构造函数。
     */
    SparseImage();
    
    /**
     * @brief Destructor.
     *        析构函数。
     */
    ~SparseImage();
    
    /**
     * @brief Pre-load sparse image file.
     *        预加载稀疏镜像文件。
     * @param szSparseFile [in] Path to the sparse image file. 稀疏镜像文件路径。
     * @return Status code. 错误代码。
     */
    int PreLoadImage(TCHAR *szSparseFile);
    
    /**
     * @brief Pre-load sparse image file (modern version).
     *        预加载稀疏镜像文件（现代版本）。
     * @param szSparseFile [in] Path to the sparse image file. 稀疏镜像文件路径。
     * @return Status code. 错误代码。
     */
    int PreLoadImage(const std::string& szSparseFile);
    
    /**
     * @brief Program the sparse image to device.
     *        将稀疏镜像编程到设备。
     * @param pProtocol [in] Protocol pointer for device communication. 用于设备通信的协议指针。
     * @param dwOffset [in] Offset to write the image. 写入镜像的偏移量。
     * @return Status code. 错误代码。
     */
    int ProgramImage(Protocol *pProtocol, int64_t dwOffset);

private:
    SPARSE_HEADER SparseHeader;  // Sparse image header / 稀疏镜像头
    HANDLE hSparseImage;         // Handle to the sparse image file / 稀疏镜像文件句柄
    bool bSparseImage;           // Flag indicating if image is sparse / 标识镜像是否为稀疏格式的标志
};
