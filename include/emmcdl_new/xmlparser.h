/*****************************************************************************
 * xmlparser.h
 *
 * This file implements XML parsing functionality for device flashing
 * 本文件实现了设备刷机的 XML 解析功能
 *
 * This module provides a lightweight XML parser for parsing configuration
 * files used in device flashing operations. It supports parsing strings,
 * integers, and simple mathematical expressions.
 * 此模块提供了一个轻量级的 XML 解析器，用于解析设备刷机操作中使用的
 * 配置文件。它支持解析字符串、整数和简单的数学表达式。
 *
 *****************************************************************************/

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

/**
 * @class XMLParser
 * @brief Lightweight XML parser for configuration files.
 *        用于配置文件的轻量级 XML 解析器。
 *
 * This class provides functionality to parse XML configuration files
 * used in device flashing operations. It supports parsing strings,
 * integers, and simple mathematical expressions.
 * 本类提供解析设备刷机操作中使用的 XML 配置文件的功能。
 * 它支持解析字符串、整数和简单的数学表达式。
 */
class XMLParser {
public:
    static constexpr int64_t NUM_DISK_SECTORS = 512;  // Number of disk sectors / 磁盘扇区数
    static std::function<uint16_t(uint8_t*, int32_t)>* crc16;  // CRC16 function pointer / CRC16 函数指针

    /**
     * @brief Constructor.
     *        构造函数。
     */
    XMLParser();
    
    /**
     * @brief Destructor.
     *        析构函数。
     */
    ~XMLParser();
    
    /**
     * @brief Load XML file.
     *        加载 XML 文件。
     * @param fname [in] Path to the XML file. XML 文件路径。
     * @return Status code. 错误代码。
     */
    int LoadXML(char* fname);
    
    /**
     * @brief Load XML file (modern version).
     *        加载 XML 文件（现代版本）。
     * @param fname [in] Path to the XML file. XML 文件路径。
     * @return Status code. 错误代码。
     */
    int LoadXML(std::string fname);
    
    /**
     * @brief Parse a string from XML line.
     *        从 XML 行解析字符串。
     * @param line [in] Input line. 输入行。
     * @param key [in] Key to find. 要查找的键。
     * @param value [out] Output value. 输出值。
     * @return Status code. 错误代码。
     */
    int ParseXMLString(char* line, char* key, char* value);
    
    /**
     * @brief Parse a string from XML line (modern version).
     *        从 XML 行解析字符串（现代版本）。
     * @param line [in] Input line. 输入行。
     * @param key [in] Key to find. 要查找的键。
     * @param value [out] Output value. 输出值。
     * @return Status code. 错误代码。
     */
    int ParseXMLString(const std::string& line, const std::string& key, std::string& value);
    
    /**
     * @brief Parse an integer from XML line.
     *        从 XML 行解析整数。
     * @param line [in] Input line. 输入行。
     * @param key [in] Key to find. 要查找的键。
     * @param value [out] Output value. 输出值。
     * @return Status code. 错误代码。
     */
    int ParseXMLInteger(char* line, const char* key, uint64_t* value);
    
    /**
     * @brief Parse an integer from XML line with simple expression evaluation.
     *        从 XML 行解析整数，支持简单的表达式计算。
     * @param line [in] Input line. 输入行。
     * @param key [in] Key to find. 要查找的键。
     * @param value [out] Output value. 输出值。
     * @return Status code. 错误代码。
     */
    int ParseXMLInteger(const std::string& line, const std::string& key, int64_t& value);
    
    /**
     * @brief Replace a substring in a C string.
     *        替换 C 字符串中的子字符串。
     * @param inp [in] Input string. 输入字符串。
     * @param find [in] String to find. 要查找的字符串。
     * @param rep [in] Replacement string. 替换字符串。
     * @return Modified string. 修改后的字符串。
     */
    char* StringReplace(char* inp, const char* find, const char* rep);
    
    /**
     * @brief Replace a substring in a std::string.
     *        替换 std::string 中的子字符串。
     * @param inp [in] Input string. 输入字符串。
     * @param find [in] String to find. 要查找的字符串。
     * @param rep [in] Replacement string. 替换字符串。
     * @return Modified string. 修改后的字符串。
     */
    std::string StringReplace(const std::string& inp, const std::string& find, const std::string& rep);
    
    /**
     * @brief Set a value in a key-value pair string.
     *        在键值对字符串中设置值。
     * @param key [in] Key string. 键字符串。
     * @param keyName [in] Key name. 键名。
     * @param value [in] Value to set. 要设置的值。
     * @return Modified string. 修改后的字符串。
     */
    char* StringSetValue(char* key, char* keyName, char* value);
    
    /**
     * @brief Set a value in a key-value pair std::string.
     *        在键值对 std::string 中设置值。
     * @param key [in] Key string. 键字符串。
     * @param keyName [in] Key name. 键名。
     * @param value [in] Value to set. 要设置的值。
     * @return Modified string. 修改后的字符串。
     */
    std::string StringSetValue(const std::string& key, const std::string& keyName, const std::string& value);

    /**
     * @brief Parse and evaluate a simple mathematical expression.
     *        解析并计算一个简单的数学表达式。
     * @param expr [in] Expression string. 表达式字符串。
     * @param value [out] Calculated result. 计算结果。
     * @return Status code. 错误代码。
     */
    int ParseXMLEvaluate(char* expr, uint64_t& value);
    
    /**
     * @brief Parse and evaluate a simple mathematical expression (modern version).
     *        解析并计算一个简单的数学表达式（现代版本）。
     * @param expr [in] Expression string. 表达式字符串。
     * @param value [out] Calculated result. 计算结果。
     * @return Status code. 错误代码。
     */
    int ParseXMLEvaluate(std::string expr, int64_t& value);

private:
    char* xmlStart;          // Start of XML data / XML 数据起始位置
    char* xmlEnd;            // End of XML data / XML 数据结束位置
    char* keyStart;          // Start of key data / 键数据起始位置
    char* keyEnd;            // End of key data / 键数据结束位置
    std::string xmlContent;   // XML content string / XML 内容字符串
};
