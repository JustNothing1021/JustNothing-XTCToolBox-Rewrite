#pragma once

#include <string>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <tchar.h>
#include "emmcdl_new/protocol.h"

#define SPARSE_MAGIC      0xED26FF3A
#define SPARSE_RAW_CHUNK  0xCAC1
#define SPARSE_FILL_CHUNK 0xCAC2
#define SPARSE_DONT_CARE  0xCAC3

// Security Header struct. The first data read in from the FFU.
typedef struct _SPARSE_HEADER {
    uint32_t dwMagic;           // 0xed26ff3a
    uint16_t wVerMajor;         // 0x1 Major version, reject version with higher major version
    uint16_t wVerMinor;         // 0x0 Minor version, accept version with higher minor version
    uint16_t wSparseHeaderSize; // 28 bytes for first revision of the file format
    uint16_t wChunkHeaderSize;  // 12 bytes for first revision of the file format
    uint32_t dwBlockSize;       // 4096 block size in bytes must be multiple of 4
    uint32_t dwTotalBlocks;     // Total blocks in the non-sparse output file
    uint32_t dwTotalChunks;     // Total chunks in the sparse input image
    uint32_t dwImageChecksum;   // CRC32 checksum of the original data counting do not care 802.3 polynomial
} SPARSE_HEADER;

typedef struct _CHUNK_HEADER {
    uint16_t wChunkType;        // 0xCAC1 -> raw; 0xCAC2 -> fill; 0xCAC3 -> don't care
    uint16_t wReserved;         // Reserved should be all 0
    uint32_t dwChunkSize;       // Number of blocks this chunk takes in output image
    uint32_t dwTotalSize;       // Number of bytes in input file including chunk header round up to next block size
} CHUNK_HEADER;

class SparseImage {
public:
    int PreLoadImage(TCHAR *szSparseFile);
    int PreLoadImage(std::string szSparseFile);
    int ProgramImage(Protocol *pProtocol, int64_t dwOffset);
    SparseImage();
    ~SparseImage();

private:
    SPARSE_HEADER SparseHeader;
    HANDLE hSparseImage;
    bool bSparseImage;
};