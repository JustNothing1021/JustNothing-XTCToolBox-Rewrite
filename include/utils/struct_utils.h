
#pragma once
#ifndef STRUCT_UTILS_H
#define STRUCT_UTILS_H

#include <string>
#include <cstring>
#include <vector>
#include <iostream>
#include <cstdint>

namespace struct_utils {

    /**
     * 将一段二进制数值转换为指定类型的结构体。
     * @param data 二进制数据
     */
    template <typename T>
    T from_binary(uint8_t* data);

    /**
     * 将指定类型的结构体转换为一段二进制数值。
     * @param data 结构体数据
     * @param buffer 二进制缓冲区
     * @return 转换后的二进制数据长度
     */
    template <typename T>
    size_t to_binary(T data, uint8_t* buffer);

    
}


#endif // STRUCT_UTILS_H