

#include "emmcdl_new/partition.h"
#include "emmcdl_new/protocol.h"
#include "emmcdl_new/sparse.h"
#include "emmcdl_new/utils.h"
#include "utils/string_utils.h"
#include "utils/logger.h"


using namespace std;

string StringSetValue(std::string inp, const std::string find, const std::string rep) {
    while (true) {
        size_t pos = inp.find(find);
        if (pos == std::string::npos) break;
        inp.replace(pos, find.length(), rep);
    }
    return inp;
}


int Partition::ParseXMLEvaluate(std::string expr, uint64_t& value, PartitionEntry* pe) {

    // 全词匹配
    function<size_t(const string&, const string&, size_t, const string&)> fullyMatch =
        [&](const string& str, const string& pattern, size_t offset, const string& ignores) -> size_t {
        size_t res = str.find(pattern, offset);
        if (res == string::npos) return res;
        if (res > 0) {
            char prev = str[res - 1];
            if (isspace(prev) || ignores.find(prev) != string::npos) {
                return res;
            } else {
                return string::npos;
            }
        }
        if (res + pattern.length() < str.length()) {
            char next = str[res + pattern.length()];
            if (isspace(next) || ignores.find(next) != string::npos) {
                return res;
            } else {
                return string::npos;
            }
        }
        return res;
        };


    // 替换NUM_DISK_SECTORS
    while (true) {
        size_t pos = fullyMatch(expr, "NUM_DISK_SECTORS", 0, " \t\r\n+-*/");
        if (pos == std::string::npos) break;
        expr.replace(pos, strlen("NUM_DISK_SECTORS"), to_string(NUM_DISK_SECTORS));
    }

    // 处理CRC32(offset,length)
    bool isFirstWarnCRC32 = true;
    while (true) {
        size_t pos = fullyMatch(expr, "CRC32", 0, " (+-*/)=\"\t\r\n");
        if (pos == string::npos) break;
        size_t startPos = expr.find("(", pos);
        if (startPos == string::npos) {
            LERROR("Partition::ParseXMLEvaluate", "表达式\"%s\"中的CRC32缺少左括号，"
                "位于第%d个字符，返回ERROR_INVALID_DATA", expr.c_str(), pos);
            return ERROR_INVALID_DATA;
        }
        size_t endPos = expr.find(")", startPos);
        if (endPos == string::npos) {
            LERROR("Partition::ParseXMLEvaluate", "表达式\"%s\"中的CRC32缺少右括号，"
                "位于第%d个字符，返回ERROR_INVALID_DATA", expr.c_str(), pos);
            return ERROR_INVALID_DATA;
        }
        string crcExpr = expr.substr(startPos + 1, endPos - startPos - 1);
        if (crcExpr.find(",") == string::npos) {
            LERROR("Partition::ParseXMLEvaluate", "表达式\"%s\"中的CRC32参数(\"%s\")缺少逗号，"
                "位于第%d个字符，返回ERROR_INVALID_DATA", expr.c_str(), crcExpr.c_str(), pos);
            return ERROR_INVALID_DATA;
        }
        LDEBUG("Partition::ParseXMLEvaluate", "本行使用了CRC32(%s)", crcExpr);
        size_t commaPos = crcExpr.find(",");
        string offsetExpr = crcExpr.substr(0, commaPos);
        string lengthExpr = crcExpr.substr(commaPos + 1);
        uint64_t crc;
        uint64_t offset, length;
        if (ParseXMLEvaluate(offsetExpr, offset, pe) != ERROR_SUCCESS) {
            LERROR("Partition::ParseXMLEvaluate", "无法计算表达式\"%s\"中的CRC32偏移量部分\"%s\"，"
                "返回ERROR_INVALID_DATA", expr.c_str(), offsetExpr.c_str());
            return ERROR_INVALID_DATA;
        }
        if (ParseXMLEvaluate(lengthExpr, length, pe) != ERROR_SUCCESS) {
            LERROR("Partition::ParseXMLEvaluate", "无法计算表达式\"%s\"中的CRC32长度部分\"%s\"，"
                "返回ERROR_INVALID_DATA", expr.c_str(), lengthExpr.c_str());
            return ERROR_INVALID_DATA;
        }
        pe->crc_start = offset;
        pe->crc_size = length;
    }
    try {
        value = string_utils::calc_expr(expr);
    } catch (const std::exception& e) {
        LERROR("Partition::ParseXMLEvaluate", "无法计算表达式\"%s\"，错误信息：%s，返回ERROR_INVALID_DATA", expr.c_str(), e.what());
        return ERROR_INVALID_DATA;
    }
    return ERROR_SUCCESS;
}

int Partition::ParseXMLInt64(const string& line, const string& key, uint64_t& value, PartitionEntry* pe) {
    std::string tmp;
    if (ParseXMLString(line, key, tmp) != ERROR_SUCCESS) {
        LWARN("Partition::ParseXMLInt64", "无法找到键值对\"%s\"，返回ERROR_INVALID_DATA", key.c_str());
        return ERROR_INVALID_DATA;
    }
    return ParseXMLEvaluate(tmp, value, pe);
}


int Partition::Reflect(int data, int len) {
    int ref = 0;
    for (int i = 0; i < len; i++) {
        if (data & 0x1) {
            ref |= (1 << ((len - 1) - i));
        }
        data = (data >> 1);
    }
    return ref;
}

unsigned int Partition::CalcCRC32(const BYTE* buffer, int len) {
    int k = 8;                   // length of unit (i.e. BYTE)
    int MSB = 0;
    int gx = 0x04C11DB7;         // IEEE 32bit polynomial
    int regs = 0xFFFFFFFF;       // init to all ones
    int regsMask = 0xFFFFFFFF;   // ensure only 32 bit answer
    int regsMSB = 0;

    for (int i = 0; i < len; i++) {
        BYTE DataByte = buffer[i];
        DataByte = (BYTE) Reflect(DataByte, 8);
        for (int j = 0; j < k; j++) {
            MSB = DataByte >> (k - 1);  // get MSB
            MSB &= 1;                 // ensure just 1 bit
            regsMSB = (regs >> 31) & 1; // MSB of regs
            regs = regs << 1;           // shift regs for CRC-CCITT
            if (regsMSB ^ MSB) {       // MSB is a 1
                regs = regs ^ gx;       // XOR with generator poly
            }
            regs = regs & regsMask;   // Mask off excess upper bits
            DataByte <<= 1;           // get to next bit
        }
    }

    regs = regs & regsMask;       // Mask off excess upper bits
    return Reflect(regs, 32) ^ 0xFFFFFFFF;
}

uint32_t Partition::CalcCRC32(const ByteArray& buffer, int len) {
    return CalcCRC32(buffer.data(), len);
}

int Partition::ParseXMLOptions() {
    return ERROR_SUCCESS;
}

int Partition::ParsePathList() {
    return ERROR_SUCCESS;
}


bool Partition::CheckEmptyLine(std::string str) {
    size_t keylen = str.length();
    size_t index = 0;
    while (index < keylen && isspace(str[index])) {
        index++;
        keylen--;
    }
    return (keylen == 0);
}

int Partition::ParseXMLKey(const string& line, PartitionEntry* pe) {
    // 如果是空行那么什么也不干
    if (CheckEmptyLine(line)) {
        pe->eCmd = CMD_NOP;
        return ERROR_SUCCESS;
    }

    // 检查这一行执行的命令
    pe->eCmd = CMD_INVALID;
    string stripped = string_utils::strip(line);
    if (stripped.substr(0, 8) == "<data>" || stripped.substr(0, 9) == "</data>"
        || stripped.substr(0, 10) == "<patches>" || stripped.substr(0, 9) == "</patches>") {
        pe->eCmd = CMD_NOP;
        LDEBUG("Partition::ParseXMLKey", "本行是格式符或空行，不执行操作，命令设置为CMD_NOP");
        return ERROR_SUCCESS;
    } else if (stripped.substr(0, 8) == "<program") {
        pe->eCmd = CMD_PROGRAM;
        LDEBUG("Partition::ParseXMLKey", "本行命令为program，设置命令为CMD_PROGRAM");
    } else if (stripped.substr(0, 6) == "<patch") {
        pe->eCmd = CMD_PATCH;
        LDEBUG("Partition::ParseXMLKey", "本行命令为patch，设置命令为CMD_PATCH");
    } else if (stripped.substr(0, 8) == "<options") {
        pe->eCmd = CMD_OPTION;
        LDEBUG("Partition::ParseXMLKey", "本行命令为options，设置命令为CMD_OPTION");
    } else if (stripped.substr(0, 12) == "<search_path") {
        pe->eCmd = CMD_PATH;
        LDEBUG("Partition::ParseXMLKey", "本行命令为search_path，设置命令为CMD_PATH");
    } else if (stripped.substr(0, 6) == "<read") {
        pe->eCmd = CMD_READ;
        LDEBUG("Partition::ParseXMLKey", "本行命令为read，设置命令为CMD_READ");
    } else if (stripped.substr(0, 4) == "<!--" || stripped.substr(0, 3) == "-->" 
            || stripped.substr(0, 2) == "<?") {
        pe->eCmd = CMD_NOP;
        LDEBUG("Partition::ParseXMLKey", "本行是注释，不执行操作，命令设置为CMD_NOP");
        return ERROR_SUCCESS;
    } else {
        LWARN("Partition::ParseXMLKey", "无法解析本行:\n  %s\n", line.c_str());
        return ERROR_INVALID_DATA;
    }

    if (pe->eCmd == CMD_OPTION) {
        ParseXMLOptions();
    } else if (pe->eCmd == CMD_PATH) {
        ParsePathList();
    }

    // 所有的命令都需要这三个参数
    if (ParseXMLInt64(line, "start_sector", pe->start_sector, pe) != ERROR_SUCCESS) {
        LWARN("Partition::ParseXMLKey", "XML中的一行缺少start_sector字段，返回ERROR_INVALID_DATA");
        LWARN("Partition::ParseXMLKey", "原文内容:\n  %s\n", line.c_str());
        return ERROR_INVALID_DATA;
    } else {
        LDEBUG("Partition::ParseXMLKey", "本行 start_sector=%d", pe->start_sector);
    }

    uint64_t partNum;
    if (ParseXMLInt64(line, "physical_partition_number", partNum, pe) != ERROR_SUCCESS) {
        LWARN("Partition::ParseXMLKey", "XML中的一行缺少physical_partition_number字段，返回ERROR_INVALID_DATA");
        LWARN("Partition::ParseXMLKey", "原文内容:\n  %s\n", line.c_str());
    } else {
        pe->physical_partition_number = (uint8_t) partNum;
        LDEBUG("Partition::ParseXMLKey", "本行 physical_partition_number=%d", pe->physical_partition_number);
    }

    if (ParseXMLInt64(line, "num_partition_sectors", pe->num_sectors, pe) != ERROR_SUCCESS) {
        pe->num_sectors = (uint64_t) -1;
        LWARN("Partition::ParseXMLKey", "XML中的一行缺少num_partition_sectors字段，默认使用UINT64_MAX");
    } else {
        LDEBUG("Partition::ParseXMLKey", "本行 num_partition_sectors=%d", pe->num_sectors);
    }

    if (pe->eCmd == CMD_PATCH || pe->eCmd == CMD_PROGRAM || pe->eCmd == CMD_READ) {
        // Both program and patch need a filename to be valid
        LDEBUG("Partition::ParseXMLKey", "解析patch, program或read的特有字段");

        if (ParseXMLString(line, "filename", pe->filename) != ERROR_SUCCESS) {
            LWARN("Partition::ParseXMLKey", "XML中的一行缺少filename字段，"
                "无法为program, patch或read命令继续，返回ERROR_INVALID_DATA");
            LWARN("Partition::ParseXMLKey", "原文内容:\n  %s\n", line.c_str());
            return ERROR_INVALID_DATA;
        } else {
            if (pe->filename.empty()) {
                pe->eCmd = CMD_NOP;
                LWARN("Partition::ParseXMLKey", "XML中的一行filename字段为空，"
                    "不执行任何操作，命令设置为CMD_NOP");
                LWARN("Partition::ParseXMLKey", "原文内容:\n  %s\n", line.c_str());
                return ERROR_SUCCESS;
            }
        }

        // File sector offset is optional for both these otherwise use default
        if (ParseXMLInt64(line, "file_sector_offset", pe->offset, pe) == ERROR_SUCCESS) {
            LDEBUG("Partition::ParseXMLKey", "本行 file_sector_offset=%d", pe->offset);
        } else {
            pe->offset = (uint64_t) -1;
            LDEBUG("Partition::ParseXMLKey", "XML中的一行缺少file_sector_offset字段，"
                "默认使用UINT64_MAX");
        }

        // The following entries should only be used in patching
        if (pe->eCmd == CMD_PATCH) {
            LDEBUG("Partition::ParseXMLKey", "解析patch命令的特有字段");
            // Get the value parameter
            if (ParseXMLInt64(line, "value", pe->patch_value, pe) == ERROR_SUCCESS) {
                LDEBUG("Partition::ParseXMLKey", "本行 value=%d", pe->patch_value);
            } else {
                LWARN("Partition::ParseXMLKey", "XML中的一行缺少patch命令的value字段，返回ERROR_INVALID_DATA");
                LWARN("Partition::ParseXMLKey", "原文内容:\n  %s\n", line.c_str());
                return ERROR_INVALID_DATA;
            }
            LDEBUG("Partition::ParseXMLKey", "当前 crc_size=%d", (int) pe->crc_size);

            if (ParseXMLInt64(line, "byte_offset", pe->patch_offset, pe) == ERROR_SUCCESS) {
                LDEBUG("Partition::ParseXMLKey", "本行 byte_offset=%d", pe->patch_offset);
            } else {
                LWARN("Partition::ParseXMLKey", "XML中的一行缺少patch命令的byte_offset字段，返回ERROR_INVALID_DATA");
                LWARN("Partition::ParseXMLKey", "原文内容:\n  %s\n", line.c_str());
                return ERROR_INVALID_DATA;
            }

            if (ParseXMLInt64(line, "size_in_bytes", pe->patch_size, pe) == ERROR_SUCCESS) {
                LDEBUG("Partition::ParseXMLKey", "本行 size_in_bytes=%d", pe->patch_size);
            } else {
                LWARN("Partition::ParseXMLKey", "XML中的一行缺少patch命令的size_in_bytes字段，返回ERROR_INVALID_DATA");
                LWARN("Partition::ParseXMLKey", "原文内容:\n  %s\n", line.c_str());
                return ERROR_INVALID_DATA;
            }

        } // end of CMD_PATCH
    } // end of CMD_PROGRAM || CMD_PATCH

    return ERROR_SUCCESS;
}



int Partition::ProgramPartitionEntry(Protocol* proto, PartitionEntry pe, const string& key) {
    HANDLE hRead = INVALID_HANDLE_VALUE;
    bool bSparse = false;
    int status = ERROR_SUCCESS;

    if (proto == NULL) {
        LWARN("Partition::ProgramPartitionEntry", "传入的Protocol指针为空，无法继续，返回ERROR_INVALID_PARAMETER");
        return ERROR_INVALID_PARAMETER;
    }

    if (pe.filename == "ZERO") {
        LDEBUG("Partition::ProgramPartitionEntry", "当前filename为ZERO，擦除分区内容");
    } else {
        SparseImage sparse;
        status = sparse.PreLoadImage(pe.filename);
        if (status == ERROR_SUCCESS) {
            LDEBUG("Partition::ProgramPartitionEntry", "当前filename是稀疏文件");
            bSparse = true;
            status = sparse.ProgramImage(proto, pe.start_sector * proto->GetDiskSectorSize());
        } else {
            LDEBUG("Partition::ProgramPartitionEntry", "当前filename不是ZERO且不是稀疏文件");
            // Open the file that we are supposed to dump
            status = ERROR_SUCCESS;
            hRead = CreateFileA(pe.filename.c_str(),
                GENERIC_READ,
                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                NULL,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                NULL);
            if (hRead == INVALID_HANDLE_VALUE) {
                status = GetLastError();
            } else {
                // Update the number of sectors based on real file size, rounded to next sector offset
                DWORD dwUpperFileSize = 0;
                DWORD dwLowerFileSize = GetFileSize(hRead, &dwUpperFileSize);
                int64_t dwTotalSize = dwLowerFileSize + ((int64_t) dwUpperFileSize << 32);
                dwTotalSize = (dwTotalSize + proto->GetDiskSectorSize() - 1) & (int64_t) ~(proto->GetDiskSectorSize() - 1);
                dwTotalSize = dwTotalSize / proto->GetDiskSectorSize();
                if (dwTotalSize <= (int64_t) pe.num_sectors) {
                    pe.num_sectors = dwTotalSize;
                } else {
                    LWARN("Partition::ProgramPartitionEntry", "指定的分区扇区数(%llu)小于文件实际所需扇区数(%llu)，"
                        "将使用实际所需扇区数", pe.num_sectors, dwTotalSize);
                }
                status = ERROR_SUCCESS;
            }
        }
    }

    if (status == ERROR_SUCCESS && !bSparse) {
        // Fast copy from input file to output disk
        LDEBUG("Partition::ProgramPartitionEntry", "输入偏移: %llu 输出偏移: %llu 扇区数: %llu", pe.offset, pe.start_sector, pe.num_sectors);
        LDEBUG("Partition::ProgramPartitionEntry", "开始使用FastCopy写入分区数据");
        LDEBUG("Partition::ProgramPartitionEntry", "参数：offset=%llu start_sector=%llu num_sectors=%llu",
            pe.offset, pe.start_sector, pe.num_sectors);
        status = proto->FastCopy(hRead, pe.offset, proto->GetDiskHandle(), pe.start_sector, pe.num_sectors, pe.physical_partition_number);
        if (status != ERROR_SUCCESS) {
            LWARN("Partition::ProgramPartitionEntry", "FastCopy写入分区数据失败，状态：%s",
            getErrorDescription(status).c_str());
        } else {
            LDEBUG("Partition::ProgramPartitionEntry", "FastCopy写入分区数据成功");
        }
        CloseHandle(hRead);
    }
    return status;
}

int Partition::ProgramImage(Protocol* proto) {
    int status = ERROR_SUCCESS;

    PartitionEntry pe;
    string key;
    string keyName;
    string line;
    while (GetNextXMLKey(keyName, key) == ERROR_SUCCESS) {
        // parse the XML key if we don't understand it then continue
        if (ParseXMLKey(key, &pe) != ERROR_SUCCESS) {
            // If we don't understand the command just try sending it otherwise ignore command
            if (pe.eCmd == CMD_INVALID) {
                status = proto->ProgramRawCommand(key);
            }
        } else if (pe.eCmd == CMD_PROGRAM) {
            status = ProgramPartitionEntry(proto, pe, key);
        } else if (pe.eCmd == CMD_PATCH) {
            // Only patch disk entries
            if (pe.filename == "DISK") {
                status = proto->ProgramPatchEntry(pe, key);
            }
        } else if (pe.eCmd == CMD_READ) {
            // status = proto->DumpDiskContents(pe.start_sector, pe.num_sectors, pe.filename, pe.physical_partition_number, NULL);
            status = proto->DumpDiskContents(pe.start_sector, pe.num_sectors, 
                pe.filename, pe.physical_partition_number);
        } else if (pe.eCmd == CMD_ZEROOUT) {
            status = proto->WipeDiskContents(pe.start_sector, pe.num_sectors);
        }

        if (status != ERROR_SUCCESS) {
            break;
        }
    }
    CloseXML();
    return status;
}

int Partition::PreLoadImage(const string& fname) {
    HANDLE hXML;
    int status = ERROR_SUCCESS;
    xmlStart = NULL;

    // Open the XML file and read into RAM
    hXML = CreateFileA(fname.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        0,
        NULL);

    if (hXML == INVALID_HANDLE_VALUE) {
        return GetLastError();
    }

    DWORD xmlSize = GetFileSize(hXML, NULL);
    size_t sizeOut;

    // Make sure filesize is valid
    if (xmlSize == INVALID_FILE_SIZE) {
        CloseHandle(hXML);
        LWARN("Partition::PreLoadImage", "无效的文件大小, 返回INVALID_FILE_SIZE");
        return INVALID_FILE_SIZE;
    }

    char* xmlTmp = (char*) malloc(xmlSize);
    xmlStart = (char*) malloc(xmlSize + 2);
    xmlEnd = xmlStart + xmlSize;
    keyStart = xmlStart;

    if (xmlTmp == NULL || xmlStart == NULL) {
        status = ERROR_OUTOFMEMORY;
    }


    if (status == ERROR_SUCCESS) {
        if (!ReadFile(hXML, xmlTmp, xmlSize, &xmlSize, NULL)) {
            status = ERROR_FILE_INVALID;
        }
    }

    CloseHandle(hXML);

    // If successful then prep the buffer
    if (status == ERROR_SUCCESS) {

        memcpy(xmlStart, xmlTmp, xmlSize);
        xmlStart[xmlSize] = '\0';
        xmlStart[xmlSize + 1] = '\0';

        // skip over xml header should be <?xml....?>
        for (keyStart = xmlStart;keyStart < xmlEnd;) {
            if ((*keyStart++ == '<') && (*keyStart++ == '?')) break;
        }

        for (;keyStart < xmlEnd;) {
            if ((*keyStart++ == '?') && (*keyStart++ == '>')) break;
        }
        for (;keyStart < xmlEnd;) {
            if (*keyStart++ == '>') break;
        }
    }
    if (xmlTmp != NULL) free(xmlTmp);
    return status;
}

int Partition::CloseXML() {
    if (xmlStart != NULL) {
        free(xmlStart);
        xmlStart = NULL;
        xmlEnd = NULL;
        keyStart = NULL;
        keyEnd = NULL;
    }
    return ERROR_SUCCESS;
}


int Partition::GetNextXMLKey(string& keyName, string& key) {
    bool bRecordKey = true;
    for (;(keyStart < xmlEnd) && bRecordKey; keyStart++) {
        if (*keyStart == '<' && *(keyStart + 1) == '!' && *(keyStart + 2) == '-' && *(keyStart + 3) == '-') {
            for (;keyStart < xmlEnd;keyStart++) {
                if (*keyStart == '-' && *(keyStart + 1) == '-' && *(keyStart + 2) == '>') break;
            }
        } else if (*keyStart == '<') {
            for (keyEnd = keyStart; !isalnum(*keyEnd); keyEnd++);
            for (;keyEnd < xmlEnd; keyEnd++) {
                if (*keyEnd == '>' && *(keyEnd - 1) == '/') {
                    break;
                } else if (bRecordKey) {
                    if (isalnum(*keyEnd)) keyName += *keyEnd;
                    else bRecordKey = false;
                }
            }
        }
    }
    if (keyStart < xmlEnd) {
        *keyEnd = 0;
        key = std::string(keyStart, keyEnd - keyStart);
        return ERROR_SUCCESS;
    }

    return ERROR_END_OF_MEDIA;
}

