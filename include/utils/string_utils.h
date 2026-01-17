#pragma once


#ifndef STRING_UTILS_H
#define STRING_UTILS_H

#include <any>
#include <regex>
#include <string>
#include <vector>
#include <cstdio>
#include <codecvt>
#include <cstdarg>
#include <cstring>
#include <cxxabi.h>
#include <iostream>
#include <stdexcept>
#include <type_traits>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/chrono.h>
#include <unordered_map>

// 字符串处理相关工具。
namespace string_utils {

    /**
     * 将一个字符串格式化。
     * @param format 格式化字符串，语法同printf
     * @return 格式化后的字符串
     */
    std::string format(const char* format, ...);

    /**
     * 将一个字符串格式化，参数由一个map提供。
     * @param format 格式化字符串，比如 "name={name:s} age={age:02x}"，转义则输入两个大括号"{{"和"}}"
     * @param params 参数map
     * @return 格式化后的字符串
     */
    std::string format_with_map(const std::string& fmtstr, const std::unordered_map<std::string, std::any>& params);

    /**
     * 将一个字符串按照指定分隔符分割成多个子串。
     * @param str 要分割的字符串
     * @param sep 分隔符
     * @param max_split 最大分割次数，-1表示不限制
     * @param keep_sep 是否保留分隔符
     * @return 分割后的子串数组
     */

    std::vector<std::string> split(const std::string& str, const std::string& sep, long long max_split = -1, bool include_sep = false);

    /**
     * 将一个字符串列表连接成一个字符串，每个字符串之间用指定的分隔符分隔。
     * @param strs 要连接的字符串列表
     * @param sep 分隔符
     * @return 连接后的字符串
     */
    std::string join(const std::vector<std::string>& strs, const std::string& sep);

    /**
     * 将一个字符串中的指定子串替换为另一个子串。
     * @param str 要替换的字符串
     * @param old_value 要被替换的子串
     * @param new_value 替换成的子串
     * @param max_replace 最大替换次数，-1表示不限制
     * @return 替换后的字符串
     */
    std::string replace(const std::string& str, const std::string& old_value, const std::string& new_value, long long max_replace = -1);

    /**
     * 将一个宽字符串转化为普通字符串。
     * @param wstr 要转化的宽字符串
     * @return 转化后的普通字符串
     */
    std::string wstr2str(const std::wstring& wstr);

    /**
     * 将一个普通字符串转化为宽字符串。
     * @param str 要转化的普通字符串
     * @return 转化后的宽字符串
     */
    std::wstring str2wstr(const std::string& str);

    /**
     * 将一个宽字符串转化为普通字符串。
     * @param wstr 要转化的宽字符串
     * @return 转化后的普通字符串
     */
    std::string wstr2str(const wchar_t* wstr);

    /**
     * 将一个普通字符串转化为宽字符串。
     * @param str 要转化的普通字符串
     * @return 转化后的宽字符串
     */
    std::wstring str2wstr(const char* str);

    /**
     * 获取类型的可读名称。
     * @param ti 类型信息
     * @return 可读名称
     */
    std::string demangle(const std::type_info& ti);

    /**
     * 将一个any类型的值转化为字符串。
     * @param value 要转化的值
     * @param format_str 格式化字符串
     * @return 转化后的字符串
     */
    std::string format_any(const std::any& value, const std::string& format_str = "");

    /**
     * 去除一个字符串左端的某些字符。
     * @param str 字符串
     * @param whitespace 要去除的字符
     * @return 去除空白后的字符串
     */
    std::string ltrim(const std::string& str, const std::string& whitespace = " \t\n\v\f\r");

    /**
     * 去除一个字符串右端的某些字符。
     * @param str 字符串
     * @param whitespace 要去除的字符
     * @return 去除空白后的字符串
     */
    std::string rtrim(const std::string& str, const std::string& whitespace = " \t\n\v\f\r");

    /**
     * 去除一个字符串两端的某些字符。
     * @param str 要去除的字符串
     * @param whitespace 要去除的字符
     * @return 去除空白后的字符串
     */
    std::string trim(const std::string& str, const std::string& whitespace = " \t\n\v\f\r");

    /**
     * 去除一个字符串两端的空格和换行等空白字符。
     * @param str 要去除的字符串
     * @return 去除空白后的字符串
     */
    std::string strip(const std::string& str);

    /**
     * 去除一个utf-8字符串中所有的无效字符，替换为'?'。
     * @param str 要去除的字符串
     * @return 去除无效字符后的字符串
     */
    std::string remove_invalid_utf8(const std::string& str, const std::string& replace_with = "?");

    /**
     * 将一个字符串转化为十六进制表示，每个字节用两个字符表示。
     * @param str 要转化的字符串
     * @param unprintable_replace_with 无法打印的字符用这个字符替换
     */
    std::string to_hex_view(const std::string& str, char unprintable_replace_with = ' ', size_t width = 16, bool show_ascii = true);

    /**
     * 将一个字符串转化为十六进制表示，每个字节用两个字符表示。
     * @param str 要转化的字符串
     * @param unprintable_replace_with 无法打印的字符用这个字符替换
     */
    std::string to_hex_view(const char* str, size_t size, char unprintable_replace_with = ' ', size_t width = 16, bool show_ascii = true);

    /**
     * 在一个字符串中查找一个子串，要求全词匹配。
     * @note 默认忽略isspace()的字符。
     * @param str 要查找的字符串
     * @param pattern 要查找的子串
     * @param start 查找的起始位置
     * @param ignores 忽略的字符
     * @return 子串在字符串中的位置，如果未找到则返回string::npos
     */
    size_t find_fully_match(const std::string& str, const std::string& pattern, size_t start = 0, const std::string& ignores = " \t\n\v\f\r");

    /**
     * 计算一个表达式的值。
     * @note 只支持整数运算，不支持浮点数运算。
     * @param expr 表达式字符串
     * @return 表达式的值
     * @throws std::runtime_error 如果表达式无效则抛出异常
     */
    int64_t calc_expr(std::string expr,
            const std::unordered_map<std::string, int64_t>& vars = std::unordered_map<std::string, int64_t>());


}
#endif // string_UTILS_H