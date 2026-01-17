/*****************************************************************************
 * utils.h
 *
 * This file contains utility functions for error handling and string operations
 * 本文件包含错误处理和字符串操作的实用函数
 *
 * This module provides utility functions for error handling,
 * including getting error descriptions from error codes.
 * 此模块提供错误处理的实用函数，包括从错误代码获取错误描述。
 *
 *****************************************************************************/

#pragma once

#ifndef EMMCDL_UTILS_H
#define EMMCDL_UTILS_H

#include <string>
#include <locale>
#include <codecvt>
#include <windows.h>

using namespace std;

/**
 * @brief Get error description from error code.
 *        从错误代码获取错误描述。
 *
 * This function retrieves a human-readable error description
 * for a given error code using the Windows FormatMessageA function.
 * 此函数使用 Windows FormatMessageA 函数为给定的错误代码
 * 获取人类可读的错误描述。
 *
 * @param status [in] Error code. 错误代码。
 * @return Error description string. 错误描述字符串。
 */
string getErrorDescription(int status);

/**
 * @brief Replace occurrences of a substring in a C-style string.
 *        替换 C 风格字符串中的子字符串。
 *
 * This function replaces all occurrences of a substring in a C-style string
 * with another substring. The replacement is done in-place.
 * 此函数替换 C 风格字符串中所有出现的子字符串为另一个子字符串。
 * 替换是就地进行的。
 *
 * @param inp [in/out] Input string to modify. 要修改的输入字符串。
 * @param find [in] Substring to find. 要查找的子字符串。
 * @param rep [in] Substring to replace with. 要替换为的子字符串。
 * @return Pointer to the modified string. 指向修改后字符串的指针。
 */
char* StringReplace(char* inp, const char* find, const char* rep);

#endif // EMMCDL_UTILS_H
