#pragma once

#ifndef BYTEARRAY_H
#define BYTEARRAY_H

#include <string>
#include <cstring>
#include <cstdint>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <iostream>


#ifdef DATATYPES_USE_INDEPENDENT_NAMESP
#define DATATYPES_NAMESPACE_BEGIN namespace datatypes {
#define DATATYPES_NAMESPACE_END }
#else
#define DATATYPES_NAMESPACE_BEGIN
#define DATATYPES_NAMESPACE_END
#endif


DATATYPES_NAMESPACE_BEGIN

// 一个专门用于二进制数据处理的字节数组类。
class ByteArray {
    private:
        std::vector<uint8_t> _data;
        
    public:
        /**
         * 构造函数。
         * 构造一个空的ByteArray。
         */
        ByteArray() = default;
        
        /**
         * 构造函数。
         * 以一个字符串的内容构造一个ByteArray。
         * @param str 要复制的字符串
         */
        ByteArray(const std::string &str) : _data(str.begin(), str.end()) {}
        
        /**
         * 构造函数。
         * 以一个C形式字符串的内容构造一个ByteArray，遇到\0自动终止。
         * @param str 要复制的C字符串
         */
        ByteArray(const char *str) : _data(str, str + strlen(str)) {}
        
        /**
         * 构造函数。
         * 以一个字节数组的内容构造一个ByteArray。
         * @param data 要复制的字节数组
         * @param size 要复制的字节数组的大小
         */
        ByteArray(const uint8_t *data, size_t size) : _data(data, data + size) {}
        
        /**
         * 构造函数。
         * 以一个C形式字符串的内容构造一个ByteArray，指定长度。
         * @param data 要复制的C字符串
         * @param size 要复制的C字符串的大小
         */
        ByteArray(const char *data, size_t size) : _data(data, data + size) {}

        /** 
         * 复制构造函数。
         * @param other 要复制的ByteArray
         */
        ByteArray(const ByteArray &other) = default;
        
        /**
         * 移动构造函数。
         * @param other 要移动的ByteArray
         */
        ByteArray(ByteArray &&other) = default;
        
        /**
         * 赋值运算符。
         * @param other 要赋值的ByteArray
         */
        ByteArray& operator=(const ByteArray &other) = default;
        
        /**
         * 移动赋值运算符。
         * @param other 要移动赋值的ByteArray
         */
        ByteArray& operator=(ByteArray &&other) = default;
        
        // 基本操作接口
        size_t size() const { return _data.size(); }
        bool empty() const { return _data.empty(); }
        void clear() { _data.clear(); }
        void resize(size_t size) { _data.resize(size); }
        void reserve(size_t size) { _data.reserve(size); }
        
        // 数据访问
        uint8_t& operator[](size_t index) { return _data[index]; }
        const uint8_t& operator[](size_t index) const { return _data[index]; }
        uint8_t& at(size_t index) { return _data.at(index); }
        const uint8_t& at(size_t index) const { return _data.at(index); }
        
        // 迭代器支持
        auto begin() { return _data.begin(); }
        auto end() { return _data.end(); }
        auto begin() const { return _data.begin(); }
        auto end() const { return _data.end(); }
        auto cbegin() const { return _data.cbegin(); }
        auto cend() const { return _data.cend(); }
        
        // 数据操作
        void push_back(uint8_t value) { _data.push_back(value); }
        void append(const ByteArray &other) { _data.insert(_data.end(), other._data.begin(), other._data.end()); }
        void append(const uint8_t *data, size_t size) { _data.insert(_data.end(), data, data + size); }
        
        // += 运算符重载
        ByteArray& operator+=(uint8_t value) {
            _data.push_back(value);
            return *this;
        }
        
        ByteArray& operator+=(const ByteArray &other) {
            _data.insert(_data.end(), other._data.begin(), other._data.end());
            return *this;
        }
        
        ByteArray& operator+=(int value) {
            // 将int转换为字节序列（小端序）
            for (size_t i = 0; i < sizeof(int); i++) {
                _data.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
            }
            return *this;
        }
        
        ByteArray& operator+=(const char *str) {
            // 添加C字符串（不包括null终止符）
            size_t len = strlen(str);
            _data.insert(_data.end(), str, str + len);
            return *this;
        }
        
        ByteArray& operator+=(const std::string &str) {
            // 添加std::string
            _data.insert(_data.end(), str.begin(), str.end());
            return *this;
        }
        
        /**
         * 替换指定位置的字节
         * @param pos 起始位置
         * @param count 要替换的字节数
         * @param other 替换的ByteArray
         */
        void replace(size_t pos, size_t count, const ByteArray &other) {
            if (pos > _data.size()) {
                throw std::out_of_range("ByteArray::replace: position out of range");
            }
            
            // 如果替换范围超出当前大小，调整大小
            size_t end_pos = pos + count;
            if (end_pos > _data.size()) {
                _data.resize(end_pos);
            }
            
            // 替换数据
            std::copy(other._data.begin(), other._data.end(), _data.begin() + pos);
        }
        
        // 原始数据访问
        uint8_t* data() { return _data.data(); }
        const uint8_t* data() const { return _data.data(); }
        
        /**
         * 获取C风格字符串指针（兼容旧代码）
         * @return 指向内部数据的const char*指针
         */
        const char* c_str() const {
            return reinterpret_cast<const char*>(_data.data());
        }
        
        /**
         * 将ByteArray转为字符串。
         * @return ByteArray内容的字符串
         */
        std::string to_string() const {
            return std::string(_data.begin(), _data.end());
        }
        
        /**
         * 将ByteArray转为十六进制视图。
         * @param width 每行显示的字节数
         * @param show_ascii 是否依照十六进制每字节的内容显示ASCII字符
         * @param unprintable 非可打印字符的替代字符
         */
        std::string to_hex_view(size_t width = 16, bool show_ascii = true, char unprintable = ' ') const {
            std::string result;
            size_t s_size = _data.size();
            
            // 使用stringstream替代sprintf，提高性能
            std::stringstream ss;
            for (size_t i = 0; i < s_size; i += width) {
                size_t j = i;
                ss.str("");
                ss << "[0x" << std::hex << std::setw(8) << std::setfill('0') << i << "]  ";
                std::string l = ss.str();
                
                for (; j < i + width && j < s_size; j++) {
                    ss.str("");
                    ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(_data[j]) << " ";
                    l += ss.str();
                }
                
                if (j < i + width)
                    for (size_t k = j; k < i + width; k++)
                        l += "   "; // 3个空格

                if (show_ascii) {
                    l += " |  ";
                    for (int k = i; k < j; k++) {
                        if (isprint(_data[k]))
                            l += static_cast<char>(_data[k]);
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
         * 比较运算符
         */
        bool operator==(const ByteArray &other) const {
            return _data == other._data;
        }
        
        bool operator!=(const ByteArray &other) const {
            return _data != other._data;
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