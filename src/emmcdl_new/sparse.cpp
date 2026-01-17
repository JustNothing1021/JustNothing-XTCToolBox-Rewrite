
#include "emmcdl_new/sparse.h"
#include "emmcdl_new/utils.h"
#include "utils/logger.h"

using namespace std;

// Constructor
SparseImage::SparseImage() {
    bSparseImage = false;
}

// Destructor
SparseImage::~SparseImage() {
    if (bSparseImage) {
        CloseHandle(hSparseImage);
    }
}

// This will load a sparse image into memory and read headers if it is a sparse image
// RETURN: 
// ERROR_SUCCESS - Image and headers we loaded successfully
// 
int SparseImage::PreLoadImage(TCHAR* szSparseFile) {
    DWORD dwBytesRead;
    hSparseImage = CreateFileW(szSparseFile,
        GENERIC_READ,
        FILE_SHARE_READ, // We only read from here so let others open the file as well
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (hSparseImage == INVALID_HANDLE_VALUE) return GetLastError();

    // Load the sparse file header and verify it is valid
    if (ReadFile(hSparseImage, &SparseHeader, sizeof(SparseHeader), &dwBytesRead, NULL)) {
        // Check the magic number in the sparse header to see if this is a vaild sparse file
        if (SparseHeader.dwMagic != SPARSE_MAGIC) {
            CloseHandle(hSparseImage);
            return ERROR_BAD_FORMAT;
        }
    } else {
        // Couldn't read the file likely someone else has it open for exclusive access
        CloseHandle(hSparseImage);
        return ERROR_READ_FAULT;
    }

    bSparseImage = true;
    return ERROR_SUCCESS;
}

int SparseImage::PreLoadImage(const string &szSparseFile) {
    DWORD dwBytesRead;
    hSparseImage = CreateFileA(szSparseFile.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
    if (hSparseImage == INVALID_HANDLE_VALUE) return GetLastError();

    if (ReadFile(hSparseImage, &SparseHeader, sizeof(SparseHeader), &dwBytesRead, NULL)) {
        if (SparseHeader.dwMagic != SPARSE_MAGIC) {
            CloseHandle(hSparseImage);
            LERROR("SparseImage::PreLoadImage", "文件的MagicNumber(0x%x)与SPARSE_MAGIC(0x%x)不匹配", SparseHeader.dwMagic, SPARSE_MAGIC);
            return ERROR_BAD_FORMAT;
        }
    } else {
        int stat = GetLastError();
        CloseHandle(hSparseImage);
        LERROR("SparseImage::PreLoadImage", "读取文件失败，状态：%s", 
            getErrorDescription(stat).c_str());
        return stat == 0 ? ERROR_READ_FAULT : stat; // 以防GetLastError()返回0
    }

    bSparseImage = true;
    return ERROR_SUCCESS;
}

int SparseImage::ProgramImage(Protocol* pProtocol, int64_t dwOffset) {
    CHUNK_HEADER ChunkHeader;
    BYTE* bpDataBuf;
    DWORD dwBytesRead;
    DWORD dwBytesOut;
    int status = ERROR_SUCCESS;

    // Make sure we have first successfully found a sparse file and headers are loaded okay
    if (!bSparseImage) {
        LERROR("SparseImage::ProgramImage", "稀疏镜像文件未正确加载，返回ERROR_FILE_NOT_FOUND");
        return ERROR_FILE_NOT_FOUND;
    }

    // Allocated a buffer 

    // Main loop through all block entries in the sparse image
    for (uint32_t i = 0; i < SparseHeader.dwTotalChunks; i++) {
        // Read chunk header 
        if (ReadFile(hSparseImage, &ChunkHeader, sizeof(ChunkHeader), &dwBytesRead, NULL)) {
            if (ChunkHeader.wChunkType == SPARSE_RAW_CHUNK) {
                uint32_t dwChunkSize = ChunkHeader.dwChunkSize * SparseHeader.dwBlockSize;
                // Allocate a buffer the size of the chunk and read in the data
                bpDataBuf = (BYTE*) malloc(dwChunkSize);
                if (bpDataBuf == NULL) {
                    LERROR("SparseImage::ProgramImage", "处理稀疏镜像文件时内存不足，返回ERROR_OUTOFMEMORY");
                    return ERROR_OUTOFMEMORY;
                }
                if (ReadFile(hSparseImage, bpDataBuf, dwChunkSize, &dwBytesRead, NULL)) {
                    // Now we have the data so use whatever protocol we need to write this out
                    pProtocol->WriteData(bpDataBuf, dwOffset, dwChunkSize, &dwBytesOut, 0);
                    dwOffset += ChunkHeader.dwChunkSize;
                } else {
                    LERROR("SparseImage::ProgramImage", "读取稀疏镜像文件时发生错误，状态：%s", 
                        getErrorDescription(GetLastError()).c_str()
                    );
                    status = GetLastError();
                    break;
                }
                free(bpDataBuf);
            } else if (ChunkHeader.wChunkType == SPARSE_FILL_CHUNK) {
                // Fill chunk with data BYTE given for now just skip over this area
                dwOffset += ChunkHeader.dwChunkSize * SparseHeader.dwBlockSize;
            } else if (ChunkHeader.wChunkType == SPARSE_DONT_CARE) {
                // Skip the specified number of bytes in the output file
                dwOffset += ChunkHeader.dwChunkSize * SparseHeader.dwBlockSize;
            } else {
                // We have no idea what type of chunk this is return a failure and close file
                LERROR("SparseImage::ProgramImage", "遇到未知的块类型(0x%x)，返回ERROR_INVALID_DATA", ChunkHeader.wChunkType);
                status = ERROR_INVALID_DATA;
                break;
            }
        } else {
            // Failed to read data something is wrong with the file
            LERROR("SparseImage::ProgramImage", "读取文件时发生错误，状态：%s", 
                getErrorDescription(status).c_str()
            );
            status = GetLastError();
            break;
        }
    }

    // If we failed to load the file close the handle and set sparse image back to false
    if (status != ERROR_SUCCESS) {
        bSparseImage = false;
        CloseHandle(hSparseImage);
    }
    if (status == ERROR_SUCCESS) {
        LINFO("SparseImage::ProgramImage", "稀疏镜像文件写入成功");
    } else {
        LERROR("SparseImage::ProgramImage", "稀疏镜像文件写入失败，状态：%s", 
            getErrorDescription(status).c_str()
        );
    }
    return status;
}
