#include "emmcdl_new/xmlparser.h"
#include "emmcdl_new/crc.h"
#include "utils/logger.h"
#include "utils/string_utils.h"
#include "emmcdl_new/utils.h"

using namespace std;

std::function<uint16_t(uint8_t*, int)>* XMLParser::crc16 = new std::function<uint16_t(uint8_t*, int)>(CalcCRC16);

XMLParser::XMLParser() {
    xmlContent = "";
    xmlStart = NULL;
    xmlEnd = NULL;
    keyStart = NULL;
    keyEnd = NULL;
}

XMLParser::~XMLParser() {
    if (xmlStart) free(xmlStart);
}

int XMLParser::LoadXML(char* fname) {
    HANDLE hXML;
    int status = ERROR_SUCCESS;
    xmlStart = NULL;

    // Open the XML file and read into RAM
    hXML = CreateFileA(fname,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        0,
        NULL);

    if (hXML == INVALID_HANDLE_VALUE) {
        int err_stat = GetLastError();
        LERROR("XMLParser::LoadXML", "无法打开XML文件 \"%s\"，状态: %s",
            fname,
            getErrorDescription(err_stat).c_str());
        return err_stat;
    }

    DWORD xmlSize = GetFileSize(hXML, NULL);

    // Make sure filesize is valid
    if (xmlSize == INVALID_FILE_SIZE) {
        LERROR("XMLParser::LoadXML", "无法获取XML文件 \"%s\" 的大小，将会返回ERROR_FILE_INVALID");
        CloseHandle(hXML);
        return ERROR_FILE_INVALID;
    }

    xmlStart = (char*) malloc(xmlSize + 2);
    xmlEnd = xmlStart + xmlSize;
    keyStart = xmlStart;

    if (xmlStart == NULL) {
        LERROR("XMLParser::LoadXML", "无法分配内存来读取XML文件 \"%s\"，将会返回ERROR_OUTOFMEMORY");
        CloseHandle(hXML);
        return ERROR_OUTOFMEMORY;
    }


    if (status == ERROR_SUCCESS) {
        if (!ReadFile(hXML, xmlStart, xmlSize, &xmlSize, NULL)) {
            status = GetLastError();
            CloseHandle(hXML);
            LERROR("XMLParser::LoadXML", "读取XML文件 \"%s\" 时发生错误，状态: %s",
                fname,
                getErrorDescription(status).c_str());
        }
    }

    CloseHandle(hXML);

    return status;
}

int XMLParser::LoadXML(std::string fname) {
    xmlContent.clear();
    FILE* file = nullptr;
    int status = ERROR_SUCCESS;
    int errno_val = fopen_s(&file, fname.c_str(), "r");
    if (errno_val == 0 && file != nullptr) {
        fseek(file, 0, SEEK_END);
        size_t fileSize = ftell(file);
        fseek(file, 0, SEEK_SET);
        xmlContent.resize(fileSize + 1);
        size_t bytesRead = fread(&xmlContent[0], 1, fileSize, file);
        if (bytesRead != fileSize) {
            status = ERROR_READ_FAULT;
            LERROR("XMLParser::LoadXML", "读取XML文件 \"%s\" 时发生错误，读取到的字节数(%llu)与文件大小(%llu)不符，返回ERROR_READ_FAULT",
                fname.c_str(), bytesRead, fileSize);
            xmlContent.clear();
        } else {
            if (*xmlContent.end() == '\0') xmlContent.pop_back();
            xmlContent[fileSize] = '\0';
        }
        fclose(file);
    } else {
        status = errno_val;
        LERROR("XMLParser::LoadXML", "无法打开XML文件 \"%s\"，状态: %s",
            fname.c_str(),
            getErrorDescription(status).c_str());
    }
    return status;
}


// Note currently just replace in place and fill in spaces
char* XMLParser::StringSetValue(char* key, char* keyName, char* value) {
    char* sptr = strstr(key, keyName);
    if (sptr == NULL) return key;

    sptr = strstr(sptr, "\"");
    if (sptr == NULL) return key;

    // Loop through and replace with actual size;
    for (sptr++;;) {
        char tmp = *value++;
        if (*sptr == '"') break;
        if (tmp == 0) {
            *sptr++ = '"';
            for (;*sptr != '"';sptr++) {
                *sptr = ' ';
            }
            *sptr = ' ';
            break;
        }
        *sptr++ = tmp;
    }
    return key;
}

string XMLParser::StringSetValue(const string& inp, const string& find, const string& rep) {
    string result = inp;
    size_t pos = result.find(find);
    if (pos == string::npos) return result;
    pos = result.find('"', pos);
    if (pos == string::npos) return result;
    size_t endPos = result.find('"', pos + 1);
    if (endPos == string::npos) return result;
    result.replace(pos + 1, endPos - pos - 1, rep);
    return result;
}

char* XMLParser::StringReplace(char* inp, const char* find, const char* rep) {
    char tmpstr[MAX_STRING_LEN];
    int max_len = MAX_STRING_LEN;
    char* tptr = tmpstr;;
    char* sptr = strstr(inp, find);

    if (sptr != NULL) {
        // Copy part of string before the value to replace
        strncpy_s(tptr, max_len, inp, (sptr - inp));
        max_len -= (sptr - inp);
        tptr += (sptr - inp);
        sptr += strlen(find);
        // Copy the replace value
        strncpy_s(tptr, max_len, rep, strlen(rep));
        max_len -= strlen(rep);
        tptr += strlen(rep);

        // Copy the rest of the string
        strncpy_s(tptr, max_len, sptr, strlen(sptr));

        // Copy new temp string back into original
        strcpy_s(inp, MAX_STRING_LEN, tmpstr);
    }

    return inp;
}

string XMLParser::StringReplace(const string& inp, const string& find, const string& rep) {
    string result = inp;
    return string_utils::replace(result, find, rep, 1);

}

int XMLParser::ParseXMLEvaluate(char* expr, uint64_t& value) {
    // Parse simple expression understands -+/*, NUM_DISK_SECTORS,CRC32(offset:length)
    char* sptr, * sptr1, * sptr2;
    expr = StringReplace(expr, "NUM_DISK_SECTORS", "1");
    value = atoll(expr);

    sptr = strstr(expr, "CRC32");
    if (sptr != NULL) {
        char tmp[MAX_STRING_LEN];
        uint64_t crc;
        sptr1 = strstr(sptr, "(") + 1;
        if (sptr1 == NULL) {
            return ERROR_INVALID_PARAMETER;
        }
        sptr2 = strstr(sptr1, ",");
        if (sptr2 == NULL) {
            return ERROR_INVALID_PARAMETER;
        }
        strncpy_s(tmp, MAX_STRING_LEN, sptr1, (sptr2 - sptr1));
        ParseXMLEvaluate(tmp, crc);
        sptr1 = sptr2 + 1;
        sptr2 = strstr(sptr1, ")");
        if (sptr2 == NULL) {
            return ERROR_INVALID_PARAMETER;
        }
        strncpy_s(tmp, MAX_STRING_LEN, sptr1, (sptr2 - sptr1));
        ParseXMLEvaluate(tmp, crc);
        // Revome the CRC part set value 0
        memset(sptr, ' ', (sptr2 - sptr + 1) * 2);
        value = 0;
    }

    sptr = strstr(expr, "*");
    if (sptr != NULL) {
        // Found * do multiplication
        char val1[64];
        char val2[64];
        strncpy_s(val1, 64, expr, (sptr - expr));
        strcpy_s(val2, 64, sptr + 1);
        value = atoll(val1) * atoll(val2);
    }

    sptr = strstr(expr, "/");
    if (sptr != NULL) {
        // Found / do division
        char val1[64];
        char val2[64];
        strncpy_s(val1, 64, expr, (sptr - expr));
        strcpy_s(val2, 64, sptr + 1);

        // Prevent division by 0
        if (atoll(val2) > 0) {
            value = atoll(val1) / atoll(val2);
        } else {
            value = 0;
        }
    }

    sptr = strstr(expr, "-");
    if (sptr != NULL) {
        // Found - do subtraction
        char val1[32];
        char val2[32];
        strncpy_s(val1, 32, expr, (sptr - expr));
        strcpy_s(val2, 32, sptr + 1);
        value = atoll(val1) - atoll(val2);
    }

    sptr = strstr(expr, "+");
    if (sptr != NULL) {
        // Found + do addition
        char val1[32];
        char val2[32];
        strncpy_s(val1, 32, expr, (sptr - expr));
        strcpy_s(val2, 32, sptr + 1);
        value = atoll(val1) + atoll(val2);
    }

    return ERROR_SUCCESS;
}


int XMLParser::ParseXMLEvaluate(std::string expr, int64_t& value) {

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
    bool isFirstWarnSectors = true;
    while (true) {
        size_t pos = fullyMatch(expr, "NUM_DISK_SECTORS", 0, " \t\r\n+-*/");
        if (pos == std::string::npos) break;
        expr.replace(pos, strlen("NUM_DISK_SECTORS"), to_string(NUM_DISK_SECTORS));
        if (isFirstWarnSectors) {
            isFirstWarnSectors = false;
            LWARN("XMLParser::ParseXMLEvaluate", "由于暂未实现，表达式\"%s\"中的NUM_DISK_SECTORS被替换为%d",
                expr.c_str(), NUM_DISK_SECTORS);
        }
    }

    // 处理CRC32(offset,length)
    bool isFirstWarnCRC32 = true;
    while (true) {
        size_t pos = fullyMatch(expr, "CRC32", 0, " (+-*/)=\"\t\r\n");
        if (pos == string::npos) break;
        size_t startPos = expr.find("(", pos);
        if (startPos == string::npos) {
            LERROR("XMLParser::ParseXMLEvaluate", "表达式\"%s\"中的CRC32缺少左括号，"
                "位于第%d个字符，返回ERROR_INVALID_DATA", expr.c_str(), pos);
            return ERROR_INVALID_DATA;
        }
        size_t endPos = expr.find(")", startPos);
        if (endPos == string::npos) {
            LERROR("XMLParser::ParseXMLEvaluate", "表达式\"%s\"中的CRC32缺少右括号，"
                "位于第%d个字符，返回ERROR_INVALID_DATA", expr.c_str(), pos);
            return ERROR_INVALID_DATA;
        }
        string crcExpr = expr.substr(startPos + 1, endPos - startPos - 1);
        if (crcExpr.find(",") == string::npos) {
            LERROR("XMLParser::ParseXMLEvaluate", "表达式\"%s\"中的CRC32参数(\"%s\")缺少逗号，"
                "位于第%d个字符，返回ERROR_INVALID_DATA", expr.c_str(), crcExpr.c_str(), pos);
            return ERROR_INVALID_DATA;
        }
        size_t commaPos = crcExpr.find(",");
        string offsetExpr = crcExpr.substr(0, commaPos);
        string lengthExpr = crcExpr.substr(commaPos + 1);
        uint64_t crc;
        int64_t offset, length;
        if (ParseXMLEvaluate(offsetExpr, offset) != ERROR_SUCCESS) {
            LERROR("XMLParser::ParseXMLEvaluate", "无法计算表达式\"%s\"中的CRC32偏移量部分\"%s\"，"
                "返回ERROR_INVALID_DATA", expr.c_str(), offsetExpr.c_str());
            return ERROR_INVALID_DATA;
        }
        if (ParseXMLEvaluate(lengthExpr, length) != ERROR_SUCCESS) {
            LERROR("XMLParser::ParseXMLEvaluate", "无法计算表达式\"%s\"中的CRC32长度部分\"%s\"，"
                "返回ERROR_INVALID_DATA", expr.c_str(), lengthExpr.c_str());
            return ERROR_INVALID_DATA;
        }
        // crc = CalcCRC16(offset, length);
        // 源代码直接将CRC部分替换为空格，虽然我也不知道为什么要这样
        // TODO: 这里应该计算实际的CRC值
        crc = (*crc16)(nullptr, 0);
        expr.replace(pos, endPos - pos + 1, " ");
        if (isFirstWarnCRC32) {
            isFirstWarnCRC32 = false;
            LWARN("XMLParser::ParseXMLEvaluate", "由于暂未实现，表达式\"%s\"中的CRC32校验函数被替换为%d",
                expr.c_str(), crc);
        }
    }
    try {
        value = string_utils::calc_expr(expr);
    } catch (const std::exception& e) {
        LERROR("XMLParser::ParseXMLEvaluate", "无法计算表达式\"%s\"，错误信息：%s，返回ERROR_INVALID_DATA", expr.c_str(), e.what());
        return ERROR_INVALID_DATA;
    }

    return ERROR_SUCCESS;
}

int XMLParser::ParseXMLString(char* line, char* key, char* value) {
    // Check to make sure none of the parameters are null
    if (line == NULL || key == NULL || value == NULL) {
        return ERROR_INVALID_PARAMETER;
    }

    char* eptr;
    char* sptr = strstr(line, key);
    if (sptr == NULL) return ERROR_INVALID_DATA;
    sptr = strchr(sptr, '"');
    if (sptr == NULL) return ERROR_INVALID_DATA;
    sptr++;
    eptr = strchr(sptr, '"');
    if (eptr == NULL) return ERROR_INVALID_DATA;
    // Copy the value between the quotes to output string
    strncpy_s(value, MAX_PATH, sptr, eptr - sptr);

    return ERROR_SUCCESS;
}

int XMLParser::ParseXMLString(const std::string& line, const std::string& key, std::string& value) {
    size_t pos = string_utils::find_fully_match(line, key, 0, "=");
    if (pos == std::string::npos) {
        LERROR("XMLParser::ParseXMLString", "无法找到键值对\"%s\"，返回ERROR_INVALID_DATA", key.c_str());
        return ERROR_INVALID_DATA;
    }
    pos = line.find('"', pos);
    if (pos == std::string::npos) {
        LERROR("XMLParser::ParseXMLString", "无法找到键值对\"%s\"的值/该值缺少左引号，返回ERROR_INVALID_DATA", key.c_str());
        return ERROR_INVALID_DATA;
    }
    size_t endPos = line.find('"', pos + 1);
    if (endPos == std::string::npos) {
        LERROR("XMLParser::ParseXMLString", "无法找到键值对\"%s\"的值/该值缺少右引号，返回ERROR_INVALID_DATA", key.c_str());
        return ERROR_INVALID_DATA;
    }
    value = line.substr(pos + 1, endPos - pos - 1);
    return ERROR_SUCCESS;
}

int XMLParser::ParseXMLInteger(char* line, const char* key, uint64_t* value) {
    // Check to make sure none of the parameters are null
    if (line == NULL || key == NULL || value == NULL) {
        return ERROR_INVALID_PARAMETER;
    }

    char* eptr;
    char* sptr = strstr(line, key);
    if (sptr == NULL) return ERROR_INVALID_DATA;
    sptr = strchr(sptr, '"');
    if (sptr == NULL) return ERROR_INVALID_DATA;
    sptr++;
    eptr = strchr(sptr, '"');
    if (eptr == NULL) return ERROR_INVALID_DATA;
    // Copy the value between the quotes to output string
    char tmpVal[MAX_STRING_LEN] = { 0 };
    strncpy_s(tmpVal, sizeof(tmpVal), sptr, min(MAX_STRING_LEN - 1, static_cast<int>(eptr - sptr)));
    return ParseXMLEvaluate(tmpVal, *value);
}

int XMLParser::ParseXMLInteger(const std::string& line, const std::string& key, int64_t& value) {
    size_t pos = string_utils::find_fully_match(line, key, 0, ",<>+-*/");
    if (pos == std::string::npos) {
        LERROR("XMLParser::ParseXMLInteger", "无法找到键值对\"%s\"，返回ERROR_INVALID_DATA", key.c_str());
        return ERROR_INVALID_DATA;
    }
    pos = line.find('"', pos);
    if (pos == std::string::npos) {
        LERROR("XMLParser::ParseXMLInteger", "无法找到键值对\"%s\"的值/该值缺少左引号，返回ERROR_INVALID_DATA", key.c_str());
        return ERROR_INVALID_DATA;
    }
    size_t endPos = line.find('"', pos + 1);
    if (endPos == std::string::npos) {
        LERROR("XMLParser::ParseXMLInteger", "无法找到键值对\"%s\"的值/该值缺少右引号，返回ERROR_INVALID_DATA", key.c_str());
        return ERROR_INVALID_DATA;
    }
    std::string val = line.substr(pos + 1, endPos - pos - 1);
    return ParseXMLEvaluate(val, value);
}