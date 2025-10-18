#pragma once


#include <windows.h>
#include <stdint.h>
#include <stack>
#include <vector>
#include <stdio.h>
#include <algorithm>
#include <tchar.h>
#include <winerror.h>
#include <string>
#include <stdlib.h>
#include <functional>


#define MAX_STRING_LEN 512

class XMLParser {
public:
    static constexpr int64_t NUM_DISK_SECTORS = 512;
    static std::function<uint16_t(uint8_t*, int32_t)>* crc16;

    XMLParser();
    ~XMLParser();
    int LoadXML(TCHAR* fname);
    int LoadXML(std::string fname);
    int ParseXMLString(char* line, char* key, char* value);
    /**
     * @brief 解析XML中的一个字符串。
     * @param line  [in]  输入行
     * @param key   [in]  要查找的键
     * @param value [out] 输出值
     * @return 成功返回ERROR_SUCCESS，失败返回错误码。
     */
    int ParseXMLString(const std::string& line, const std::string& key, std::string& value);
    int ParseXMLInteger(char* line, const char* key, uint64_t* value);
    /**
     * @brief 解析一行XML中的一个整数，支持简单的表达式计算。
     * @param line  [in]  输入行
     * @param key   [in]  要查找的键
     * @param value [out] 输出值
     * @return 成功返回ERROR_SUCCESS，失败返回错误码。
     */
    int ParseXMLInteger(const std::string& line, const std::string& key, int64_t& value);
    char* StringReplace(char* inp, const char* find, const char* rep);
    /**
     * @brief 在字符串中替换子串。
     * @param inp  [in]  原始字符串
     * @param find [in]  要查找的子串
     * @param rep  [in]  替换字符串
     * @return 替换后的字符串
     */
    std::string StringReplace(const std::string& inp, const std::string& find, const std::string& rep);
    char* StringSetValue(char* key, char* keyName, char* value);
    /**
     * @brief 在XML中设置一个键值对。
     * @param key     [in]  原始字符串
     * @param keyName [in]  键名
     * @param value   [in]  值
     * @return 设置后的字符串
     */
    std::string StringSetValue(const std::string& key, const std::string& keyName, const std::string& value);

    int ParseXMLEvaluate(char* expr, uint64_t& value);
    /**
     * @brief 解析并计算一个简单的数学表达式，支持加减法。
     * @param expr  [in]  表达式字符串
     * @param value [out] 计算结果
     * @return 成功返回ERROR_SUCCESS，失败返回错误码。
     */
    int ParseXMLEvaluate(std::string expr, int64_t& value);

private:
    char* xmlStart;
    char* xmlEnd;
    char* keyStart;
    char* keyEnd;
    std::string xmlContent;

};
