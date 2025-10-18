#pragma once

#ifndef BYTEARRAY_H
#define BYTEARRAY_H

#include <string>
#include <cstring>
#include <cstdint>


#ifdef DATATYPES_USE_INDEPENDENT_NAMESP
#define DATATYPES_NAMESPACE_BEGIN namespace datatypes {
#define DATATYPES_NAMESPACE_END }
#else
#define DATATYPES_NAMESPACE_BEGIN
#define DATATYPES_NAMESPACE_END
#endif


DATATYPES_NAMESPACE_BEGIN

// 一个继承自std::basic_string<uint8_t>的字符数组类。
// （被指针整疯了才做的）
class ByteArray : public std::basic_string<uint8_t> {
    public:
        /**
         * 构造函数。
         * 构造一个空的ByteArray。
         */
        ByteArray() : std::basic_string<uint8_t>() {}
        /**
         * 构造函数。
         * 以一个字符串的内容构造一个ByteArray。
         * @param str 要复制的字符串
         */
        ByteArray(const std::string &str) : std::basic_string<uint8_t>(str.begin(), str.end()) {}
        /**
         * 构造函数。
         * 以一个C形式字符串的内容构造一个ByteArray，遇到\0自动终止。
         * @param str 要复制的C字符串
         */
        ByteArray(const char *str) : std::basic_string<uint8_t>(str, str + strlen(str)) {}
        /**
         * 构造函数。
         * 以一个字节数组的内容构造一个ByteArray。
         * @param data 要复制的字节数组
         * @param size 要复制的字节数组的大小
         */
        ByteArray(const uint8_t *data, size_t size) : std::basic_string<uint8_t>(data, data + size) {}
        /**
         * 构造函数。
         * 以一个C形式字符串的内容构造一个ByteArray，指定长度。
         * @param data 要复制的C字符串
         * @param size 要复制的C字符串的大小
         */
        ByteArray(const char *data, size_t size) : std::basic_string<uint8_t>(data, data + size) {}

        /** 
         * 复制构造函数。
         * @param other 要复制的ByteArray
         */
        ByteArray(const ByteArray &other) : std::basic_string<uint8_t>(other) {}
        /**
         * 将ByteArray转为字符串。
         * @return ByteArray内容的字符串
         */
        std::string to_string() const {
            return std::string(begin(), end());
        }
        /**
         * 将ByteArray转为十六进制视图。
         * @param width 每行显示的字节数
         * @param show_ascii 是否依照十六进制每字节的内容显示ASCII字符
         * @param unprintable 非可打印字符的替代字符
         */
        std::string to_hex_view(size_t width = 16, bool show_ascii = true, char unprintable = ' ') const {
            std::string result;
            size_t s_size = size();
            char buffer[30];
            for (size_t i = 0; i < s_size; i += width) {
                size_t j = i;
                sprintf(buffer, "[0x%08x]  ", i);
                std::string l(buffer);
                for ( ; j < i + width && j < s_size; j++) {
                    sprintf(buffer, "%02x ", at(j));
                    l += buffer;
                }
                if (j < i + width)
                    for (size_t k = j; k < i + width; k++)
                        l += "   "; // 3个空格

                if (show_ascii) {
                    l += " |  ";
                    for (int k = i; k < j; k++) {
                        if (isprint(at(k)))
                            l += at(k);
                        else
                            l += unprintable;
                    }
                    for (int k = j; k < i + width; k++)
                        l += " ";
                }
                result += l + "\n";
            }
            if (!result.empty()) result.pop_back(); // 去掉最后一个换行符
            return result;
        }
        /**
         * 重载输出运算符。
         * @param os 输出流
         * @param ba 要输出的ByteArray
         * @return 原输出流
         */
        friend std::ostream& operator<<(std::ostream &os, const ByteArray &ba) {
            return os << ba.to_string();
        }

};

DATATYPES_NAMESPACE_END

#endif // BYTEARRAY_H