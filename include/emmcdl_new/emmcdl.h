/*****************************************************************************
 * emmcdl.h
 *
 * This file has all definitions for command line parameters and structures
 * 本文件包含所有命令行参数和结构的定义
 *
 * This header file defines the main entry point for the EMMC Download (EMMCDL)
 * tool, which is used for flashing and managing Qualcomm devices.
 * 此头文件定义了 EMMC 下载（EMMCDL）工具的主入口点，
 * 该工具用于刷机和管理高通设备。
 *
 * Copyright (c) 2007-2011
 * Qualcomm Technologies Incorporated.
 * All Rights Reserved.
 * Qualcomm Confidential and Proprietary
 *
 *****************************************************************************/
/*=============================================================================
                        Edit History

$Header: //source/qcom/qct/platform/uefi/workspaces/pweber/apps/8x26_emmcdl/emmcdl/main/latest/inc/emmcdl.h#3 $
$DateTime: 2014/06/11 17:27:14 $ $Author: pweber $

when       who     what, where, why
-------------------------------------------------------------------------------
11/28/11   pgw     Initial version.
=============================================================================*/
#pragma once

#include <fcntl.h>
#include <windows.h>

// Protocol type definitions
// 协议类型定义
#define STREAMING_PROTOCOL  1  // Streaming protocol / 流式协议
#define FIREHOSE_PROTOCOL   2  // Firehose protocol / Firehose 协议

// Configuration constants
// 配置常量
#define MAX_XML_CNT         8     // Maximum number of XML files / XML 文件的最大数量
#define MAX_RAW_DATA_LEN  2048   // Maximum length of raw data / 原始数据的最大长度

/**
 * @enum emmc_cmd_e
 * @brief EMMC command enumeration.
 *        EMMC 命令枚举。
 *
 * This enumeration defines the types of commands that can be executed
 * by the EMMC Download tool.
 * 此枚举定义了 EMMC 下载工具可以执行的命令类型。
 */
enum emmc_cmd_e {
    EMMC_CMD_NONE,        // No command / 无命令
    EMMC_CMD_LIST,        // List devices / 列出设备
    EMMC_CMD_DUMP,        // Dump data / 转储数据
    EMMC_CMD_ERASE,       // Erase data / 擦除数据
    EMMC_CMD_WRITE,       // Write data / 写入数据
    EMMC_CMD_WIPE,        // Wipe disk / 擦除磁盘
    EMMC_CMD_GPP,         // Create GPP partitions / 创建 GPP 分区
    EMMC_CMD_TEST,        // Test command / 测试命令
    EMMC_CMD_WRITE_GPT,    // Write GPT / 写入 GPT
    EMMC_CMD_RESET,       // Reset device / 重置设备
    EMMC_CMD_FFU,         // FFU operations / FFU 操作
    EMMC_CMD_LOAD_MRPG,    // Load MRPG / 加载 MRPG
    EMMC_CMD_GPT,         // GPT operations / GPT 操作
    EMMC_CMD_SPLIT_FFU,    // Split FFU / 分割 FFU
    EMMC_CMD_RAW,         // Raw operations / 原始操作
    EMMC_CMD_LOAD_FFU,     // Load FFU / 加载 FFU
    EMMC_CMD_INFO         // Device info / 设备信息
};

/**
 * @brief Main entry point for EMMC Download tool.
 *        EMMC 下载工具的主入口点。
 *
 * This function is the main entry point for the EMMC Download tool.
 * It parses command line arguments and executes the appropriate commands.
 * 此函数是 EMMC 下载工具的主入口点。它解析命令行参数并执行相应的命令。
 *
 * @param argc [in] Argument count. 参数数量。
 * @param argv [in] Argument vector. 参数向量。
 * @return Exit code. 退出代码。
 */
int emmcdl_main(int argc, char *argv[]);
