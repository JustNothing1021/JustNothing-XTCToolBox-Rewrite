/*****************************************************************************
 * emmcdl.cpp
 *
 * This file implements the entry point for the console application.
 *
 * Copyright (c) 2007-2011
 * Qualcomm Technologies Incorporated.
 * All Rights Reserved.
 * Qualcomm Confidential and Proprietary
 *
 *****************************************************************************/
 /*=============================================================================
                         Edit History

 $Header: //source/qcom/qct/platform/uefi/workspaces/pweber/apps/8x26_emmcdl/emmcdl/main/latest/src/emmcdl.cpp#17 $
 $DateTime: 2015/05/07 21:41:17 $ $Author: pweber $

 when       who     what, where, why
 -------------------------------------------------------------------------------
 06/28/11   pgw     Call the proper function to wipe the layout rather than manually doing it
 05/18/11   pgw     Now supports gpt/crc and changed to use udpated API's
 03/21/11   pgw     Initial version.
 =============================================================================*/

#include "emmcdl_new/utils.h"
#include "emmcdl_new/targetver.h"
#include "emmcdl_new/emmcdl.h"
#include "emmcdl_new/partition.h"
#include "emmcdl_new/diskwriter.h"
#include "emmcdl_new/dload.h"
#include "emmcdl_new/sahara.h"
#include "emmcdl_new/serialport.h"
#include "emmcdl_new/firehose.h"
#include "emmcdl_new/ffu.h"
#include "utils/logger.h"
#include "utils/string_utils.h"
#include <winerror.h>

using namespace std;

static int m_protocol = FIREHOSE_PROTOCOL;
static int m_chipset = 8974;
static int m_sector_size = 512;
static bool m_emergency = false;
static bool m_verbose = false;
static SerialPort m_port;
static fh_configure_t m_cfg = { 4, "emmc", false, false, true, -1, 1024 * 1024 };


/**
 * @brief Prints the help message.
 */
int PrintHelp() {
    printf("Usage: emmcdl <option> <value>\n");
    printf("       Options:\n");
    printf("       -l                             List available mass storage devices\n");
    printf("       -info                          List HW information about device attached to COM (eg -p COM8 -info)\n");
    printf("       -MaxPayloadSizeToTargetInBytes The max bytes in firehose mode (DDR or large IMEM use 16384, default=8192)\n");
    printf("       -SkipWrite                     Do not write actual data to disk (use this for UFS provisioning)\n");
    printf("       -SkipStorageInit               Do not initialize storage device (use this for UFS provisioning)\n");
    printf("       -MemoryName <ufs/emmc>         Memory type default to emmc if none is specified\n");
    printf("       -SetActivePartition <num>      Set the specified partition active for booting\n");
    printf("       -disk_sector_size <int>        Dump from start sector to end sector to file\n");
    printf("       -d <start> <end>               Dump from start sector to end sector to file\n");
    printf("       -d <PartName>                  Dump entire partition based on partition name\n");
    printf("       -e <start> <num>               Erase disk from start sector for number of sectors\n");
    printf("       -e <PartName>                  Erase the entire partition specified\n");
    printf("       -s <sectors>                   Number of sectors in disk image\n");
    printf("       -p <port or disk>              Port or disk to program to (eg COM8, for PhysicalDrive1 use 1)\n");
    printf("       -o <filename>                  Output filename\n");
    printf("       -x <*.xml>                     Program XML file to output type -o (output) -p (port or disk)\n");
    printf("       -f <flash programmer>          Flash programmer to load to IMEM eg MPRG8960.hex\n");
    printf("       -i <singleimage>               Single image to load at offset 0 eg 8960_msimage.mbn\n");
    printf("       -t                             Run performance tests\n");
    printf("       -b <prtname> <binfile>         Write <binfile> to GPT <prtname>\n");
    printf("       -g GPP1 GPP2 GPP3 GPP4         Create GPP partitions with sizes in MB\n");
    printf("       -gq                            Do not prompt when creating GPP (quiet)\n");
    printf("       -r                             Reset device\n");
    printf("       -ffu <*.ffu>                   Download FFU image to device in emergency download need -o and -p\n");
    printf("       -splitffu <*.ffu> -o <xmlfile> Split FFU into binary chunks and create rawprogram0.xml to output location\n");
    printf("       -protocol <protocol>           Can be FIREHOSE, STREAMING default is FIREHOSE\n");
    printf("       -chipset <chipset>             Can be 8960 or 8974 familes\n");
    printf("       -gpt                           Dump the GPT from the connected device\n");
    printf("       -raw                           Send and receive RAW data to serial port 0x75 0x25 0x10\n");
    printf("       -verbose                       Enable verbose output\n");
    printf("\n\n\nExamples:");
    printf(" emmcdl -p COM8 -info\n");
    printf(" emmcdl -p COM8 -gpt\n");
    printf(" emmcdl -p COM8 -SkipWrite -SkipStorageInit -MemoryName ufs -f prog_emmc_firehose_8994_lite.mbn -x memory_configure.xml\n");
    printf(" emmcdl -p COM8 -f prog_emmc_firehose_8994_lite.mbn -x rawprogram0.xml -SetActivePartition 0\n");
    printf(" emmcdl -p COM8 -f prog_emmc_firehose_8994_lite.mbn -ffu wp8.ffu\n");
    printf(" emmcdl -p COM8 -f prog_emmc_firehose_8994_lite.mbn -d 0 1000 -o dump_1_1000.bin\n");
    printf(" emmcdl -p COM8 -f prog_emmc_firehose_8994_lite.mbn -d SVRawDump -o svrawdump.bin\n");
    printf(" emmcdl -p COM8 -f prog_emmc_firehose_8994_lite.mbn -b SBL1 c:\\temp\\sbl1.mbn\n");
    printf(" emmcdl -p COM8 -f prog_emmc_firehose_8994_lite.mbn -e 0 100\n");
    printf(" emmcdl -p COM8 -f prog_emmc_firehose_8994_lite.mbn -e MODEM_FSG\n");
    printf(" emmcdl -p COM8 -f prog_emmc_firehose_8994_lite.mbn -raw 0x75 0x25 0x10\n");
    return -1;
}

/**
 * @brief               Converts a string to a byte array.
 * @param szSerialData  Original string array
 * @param data          Output byte array
 * @param len           Length of data to convert
 */
void StringToByte(char** szSerialData, BYTE* data, int len) {
    for (int i = 0; i < len; i++) {
        int val = 0;
        if (szSerialData[i] == nullptr)
            break;
        sscanf_s(szSerialData[i], "%i", &val);
        data[i] = (BYTE) val;
    }
}


/**
 * @brief               Sends raw data to the serial port.
 * @param dnum          Device number
 * @param szSerialData  Data array to send
 * @param len           Length of data to send
 * @return              Status code
 */
int RawSerialSend(int dnum, char** szSerialData, int len) {
    int status = ERROR_SUCCESS;
    BYTE data[MAX_RAW_DATA_LEN];

    if (len < 1 || len > sizeof(data) || dnum < 1) {
        if (dnum < 1)
            LERROR("RawSerialSend", "参数无效，使用无效的端口号: %d (是不是忘了加-p参数？)", dnum);
        else
            LERROR("RawSerialSend", "参数无效，使用了无效的数据长度: %d (应在1到%d之间)", len, sizeof(data));
        return ERROR_INVALID_PARAMETER;
    }

    m_port.Open(dnum);
    StringToByte(szSerialData, data, len);
    LTRACE("RawSerialSend", "向串口发送数据:");
    string line((char*) data, len);
    try {
        LTRACE("RawSerialSend", line);
    } catch (const std::exception& e) {
        fmt::print(log_utils::Logger::TRACE.message_display_color, "  (无法显示数据)\n");
    }
    status = m_port.Write(data, len);
    BYTE rsp[2048];
    DWORD rsp_len;
    LTRACE("RawSerialSend", "等待响应...");
    status = m_port.Read(rsp, &rsp_len);
    LTRACE("RawSerialSend", "等待响应完成，状态: %s", 
            getErrorDescription(status).c_str());
    LTRACE("RawSerialSend", "接收到的响应: \n%s", string((char*) rsp, rsp_len).c_str());
    return status;
}

int LoadFlashProg(const char* mprgFile) {
    int status = ERROR_SUCCESS;

    if (m_chipset == 8974) {
        Sahara sh(&m_port);
        if (status != ERROR_SUCCESS)
            return status;
        status = sh.ConnectToDevice(true, 0);
        if (status != ERROR_SUCCESS)
            LWARN("LoadFlashProg", "通过Sahara协议连接设备出错（可能是Hello包已经被接走），将会尝试继续");
        LINFO("LoadFlashProg", "正在下载烧录内核，文件来源: \"%s\"", mprgFile);
        status = sh.LoadFlashProg(mprgFile);
        if (status != ERROR_SUCCESS)
            return status;
    } else {
        Dload dl(&m_port);
        if (status != ERROR_SUCCESS)
            return status;
        status = dl.IsDeviceInDload();
        if (status != ERROR_SUCCESS)
            return status;
        LINFO("LoadFlashProg", "正在下载烧录内核，文件来源: \"%s\"", mprgFile);
        status = dl.LoadFlashProg(mprgFile);
        if (status != ERROR_SUCCESS)
            return status;
    }
    return status;
}

int EraseDisk(uint64_t start, uint64_t num, int dnum, const char* szPartName) {
    int status = ERROR_SUCCESS;

    if (m_emergency) {
        Firehose fh(&m_port);
        fh.SetDiskSectorSize(m_sector_size);
        if (m_verbose)
            fh.EnableVerbose();
        status = fh.ConnectToFlashProg(&m_cfg);
        if (status != ERROR_SUCCESS)
            return status;
        LINFO("EraseDisk", "连接到烧录内核，开始擦除磁盘");
        fh.WipeDiskContents(start, num, szPartName);
    } else {
        DiskWriter dw;
        dw.InitDiskList(false);

        status = dw.OpenDevice(dnum);
        if (status == ERROR_SUCCESS) {
            LINFO("EraseDisk", "成功打开卷");
            LDEBUG("EraseDisk", "擦除起始扇区: %lld, 扇区数: %lld", start, num);
            status = dw.WipeDiskContents(start, num, szPartName);
        }
        dw.CloseDevice();
    }
    return status;
}

int DumpDeviceInfo(void) {
    int status = ERROR_SUCCESS;
    Sahara sh(&m_port);
    if (m_protocol == FIREHOSE_PROTOCOL) {
        pbl_info_t pbl_info;
        status = sh.DumpDeviceInfo(&pbl_info);
        if (status == ERROR_SUCCESS) {
            LINFO("GetInfo", "序列号: 0x%08x", pbl_info.serial);
            LINFO("GetInfo", "MSM硬件ID: 0x%08x", pbl_info.msm_id);
            string hash_str((char*) pbl_info.pk_hash, 32);
            LINFO("GetInfo", "OEM公钥哈希: 0x%s", hash_str.c_str());
            LINFO("GetInfo", "\nSBL软件版本: 0x%08x", pbl_info.pbl_sw);
        }
    } else {
        LWARN("GetInfo", "只有支持Sahara协议的设备才能获取此信息");
        status = ERROR_INVALID_PARAMETER;
    }

    return status;
}

int WipeDisk(int dnum) {
    DiskWriter dw;
    int status;

    dw.InitDiskList();

    status = dw.OpenDevice(dnum);
    if (status == ERROR_SUCCESS) {
        LINFO("WipeDisk", "成功打开卷");
        LINFO("WipeDisk", "正在擦除GPT和MBR");
        status = dw.WipeLayout();
    }
    dw.CloseDevice();
    return status;
}

int CreateGPP(DWORD dwGPP1, DWORD dwGPP2, DWORD dwGPP3, DWORD dwGPP4) {
    int status = ERROR_SUCCESS;

    if (m_protocol == STREAMING_PROTOCOL) {
        Dload dl(&m_port);

        status = dl.ConnectToFlashProg(4);
        if (status != ERROR_SUCCESS)
            return status;
        status = dl.OpenPartition(PRTN_EMMCUSER);
        if (status != ERROR_SUCCESS)
            return status;
        LINFO("CreateGPP", "连接到烧录内核，开始创建GPP分区");
        status = dl.CreateGPP(dwGPP1, dwGPP2, dwGPP3, dwGPP4);
    } else if (m_protocol == FIREHOSE_PROTOCOL) {
        Firehose fh(&m_port);
        fh.SetDiskSectorSize(m_sector_size);
        if (m_verbose)
            fh.EnableVerbose();
        status = fh.ConnectToFlashProg(&m_cfg);
        if (status != ERROR_SUCCESS)
            return status;
        LINFO("CreateGPP", "连接到烧录内核，开始创建GPP分区");
        status = fh.CreateGPP(dwGPP1 / 2, dwGPP2 / 2, dwGPP3 / 2, dwGPP4 / 2);
        status = fh.SetActivePartition(1);
    }

    return status;
}

int ReadGPT(int dnum) {
    int status;

    if (m_emergency) {
        Firehose fh(&m_port);
        fh.SetDiskSectorSize(m_sector_size);
        if (m_verbose)
            fh.EnableVerbose();
        status = fh.ConnectToFlashProg(&m_cfg);
        if (status != ERROR_SUCCESS)
            return status;
        LINFO("ReadGPT", "连接到烧录内核，开始读取分区表");
        fh.ReadGPT(true);
    } else {
        DiskWriter dw;
        dw.InitDiskList();
        status = dw.OpenDevice(dnum);

        if (status == ERROR_SUCCESS) {
            status = dw.ReadGPT(true);
        }

        dw.CloseDevice();
    }
    return status;
}

int WriteGPT(int dnum, const char* szPartName, const char* szBinFile) {
    int status;

    if (m_emergency) {
        Firehose fh(&m_port);
        fh.SetDiskSectorSize(m_sector_size);
        if (m_verbose)
            fh.EnableVerbose();
        status = fh.ConnectToFlashProg(&m_cfg);
        if (status != ERROR_SUCCESS)
            return status;
        LINFO("WriteGPT", "连接到烧录内核，开始写入分区表");
        status = fh.WriteGPT(szPartName, szBinFile);
    } else {
        DiskWriter dw;
        dw.InitDiskList();
        status = dw.OpenDevice(dnum);
        if (status == ERROR_SUCCESS) {
            status = dw.WriteGPT(szPartName, szBinFile);
        }
        dw.CloseDevice();
    }
    return status;
}

int ResetDevice() {
    Dload dl(&m_port);
    int status = ERROR_SUCCESS;
    if (status != ERROR_SUCCESS)
        return status;
    status = dl.DeviceReset();
    return status;
}

int FFUProgram(const char* szFFUFile) {
    FFUImage ffu;
    int status = ERROR_SUCCESS;
    Firehose fh(&m_port);
    fh.SetDiskSectorSize(m_sector_size);
    status = fh.ConnectToFlashProg(&m_cfg);
    if (status != ERROR_SUCCESS)
        return status;
    LINFO("DownloadFFU", "正在尝试打开FFU文件");
    status = ffu.PreLoadImage(szFFUFile);
    if (status != ERROR_SUCCESS)
        return status;
    LINFO("DownloadFFU", "找到有效的FFU文件，开始编程镜像");
    status = ffu.ProgramImage(&fh, 0);
    ffu.CloseFFUFile();
    return status;
}

int FFULoad(const char* szFFUFile, const char* szPartName, const char* szOutputFile) {
    int status = ERROR_SUCCESS;
    LINFO("SplitFFU", "正在加载FFU文件");
    if ((szFFUFile != NULL) && (szOutputFile != NULL)) {
        FFUImage ffu;
        ffu.SetDiskSectorSize(m_sector_size);
        status = ffu.PreLoadImage(szFFUFile);
        if (status == ERROR_SUCCESS)
            status = ffu.SplitFFUBin(szPartName, szOutputFile);
        status = ffu.CloseFFUFile();
    } else {
        return PrintHelp();
    }
    return status;
}

int FFURawProgram(const char* szFFUFile, const char* szOutputFile) {
    int status = ERROR_SUCCESS;
    LINFO("SplitFFU", "正在创建rawprogram和文件");
    if ((szFFUFile != NULL) && (szOutputFile != NULL)) {
        FFUImage ffu;
        ffu.SetDiskSectorSize(m_sector_size);
        status = ffu.FFUToRawProgram(szFFUFile, szOutputFile);
        ffu.CloseFFUFile();
    } else {
        return PrintHelp();
    }
    return status;
}

int EDownloadProgram(const char* szSingleImage, char** szXMLFile) {
    int status = ERROR_SUCCESS;
    Dload dl(&m_port);
    Firehose fh(&m_port);
    BYTE prtn = 0;

    if (szSingleImage != NULL) {
        status = dl.ConnectToFlashProg(2);
        if (status != ERROR_SUCCESS)
            return status;
        LINFO("EDownloadProgram", "连接到烧录内核，开始下载");
        dl.OpenPartition(PRTN_EMMCUSER);
        status = dl.LoadImageFile(szSingleImage);
        dl.ClosePartition();
    } else if (szXMLFile[0] != NULL) {
        if (m_protocol == STREAMING_PROTOCOL) {
            status = dl.ConnectToFlashProg(4);
            if (status != ERROR_SUCCESS)
                return status;
            LINFO("EDownloadProgram", "连接到烧录内核，开始下载");

            for (int i = 0; szXMLFile[i] != NULL; i++) {
                char szPatchFile[MAX_STRING_LEN];
                strncpy_s(szPatchFile, szXMLFile[i], sizeof(szPatchFile));
                StringReplace(szPatchFile, "rawprogram", "patch");
                char* sptr = strstr(szXMLFile[i], ".xml");
                if (sptr == NULL)
                    return ERROR_INVALID_PARAMETER;
                prtn = (BYTE) ((*--sptr) - '0' + PRTN_EMMCUSER);
                LDEBUG("SetActivePartition", "正在打开分区 %d", prtn);
                dl.OpenPartition(prtn);
                status = dl.WriteRawProgramFile(szPatchFile);
                if (status != ERROR_SUCCESS)
                    return status;
                status = dl.WriteRawProgramFile(szXMLFile[i]);
            }
            LINFO("SetActivePartition", "设置活动分区为 %d", (prtn - PRTN_EMMCUSER));
            dl.SetActivePartition();
            dl.DeviceReset();
            dl.ClosePartition();
        } else if (m_protocol == FIREHOSE_PROTOCOL) {
            fh.SetDiskSectorSize(m_sector_size);
            if (m_verbose)
                fh.EnableVerbose();
            status = fh.ConnectToFlashProg(&m_cfg);
            if (status != ERROR_SUCCESS)
                return status;
            LINFO("EDownloadProgram", "连接到烧录内核，开始下载");

            for (int i = 0; szXMLFile[i] != NULL; i++) {
                Partition rawprg(0);
                status = rawprg.PreLoadImage(szXMLFile[i]);
                if (status != ERROR_SUCCESS)
                    return status;
                status = rawprg.ProgramImage(&fh);

                char* sptr = strstr(szXMLFile[i], "rawprogram");
                if (sptr != NULL && status == ERROR_SUCCESS) {
                    Partition patch(0);
                    int pstatus = ERROR_SUCCESS;
                    char szPatchFile[MAX_STRING_LEN];
                    strncpy_s(szPatchFile, szXMLFile[i], sizeof(szPatchFile));
                    StringReplace(szPatchFile, "rawprogram", "patch");
                    pstatus = patch.PreLoadImage(szPatchFile);
                    if (pstatus == ERROR_SUCCESS)
                        patch.ProgramImage(&fh);
                }
            }

            if (m_cfg.ActivePartition >= 0) {
                status = fh.SetActivePartition(m_cfg.ActivePartition);
            }
        }
    }

    return status;
}

int RawDiskProgram(char** pFile, const char* oFile, uint64_t dnum) {
    DiskWriter dw;
    int status = ERROR_SUCCESS;

    if (oFile != NULL) {
        status = dw.OpenDiskFile(oFile, dnum);
    } else {
        int disk = dnum & 0xFFFFFFFF;
        dw.InitDiskList();
        status = dw.OpenDevice(disk);
    }
    if (status == ERROR_SUCCESS) {
        LINFO("ListDevices", "成功打开磁盘");
        for (int i = 0; pFile[i] != NULL; i++) {
            Partition p(dw.GetNumDiskSectors());
            status = p.PreLoadImage(pFile[i]);
            if (status != ERROR_SUCCESS)
                return status;
            status = p.ProgramImage(&dw);
        }
    }

    dw.CloseDevice();
    return status;
}

int RawDiskTest(int dnum, uint64_t offset) {
    DiskWriter dw;
    int status = ERROR_SUCCESS;
    offset = 0x2000000;

    dw.InitDiskList();
    status = dw.OpenDevice(dnum);
    if (status == ERROR_SUCCESS) {
        LINFO("DumpPartition", "成功打开卷");
        status = dw.DiskTest(offset);
    } else {
        LERROR("DumpPartition", "打开卷失败");
    }

    dw.CloseDevice();
    return status;
}

int RawDiskDump(uint64_t start, uint64_t num, const char* oFile, int dnum, const char* szPartName) {
    DiskWriter dw;
    int status = ERROR_SUCCESS;

    if (szPartName != nullptr) {
        LINFO("RawDiskDump", "开始转储分区 \"%s\" 的数据, 保存到\"%s\"\n", szPartName, oFile);
    } else {
        LINFO("RawDiskDump", "开始转储数据, 起始扇区=%lld, 扇区总数=%lld, 保存到\"%s\"\n", start, num, oFile);
    }
    if (m_emergency) {
        Firehose fh(&m_port);
        fh.SetDiskSectorSize(m_sector_size);
        if (m_verbose)
            fh.EnableVerbose();
        if (status != ERROR_SUCCESS)
            return status;
        status = fh.ConnectToFlashProg(&m_cfg);
        if (status != ERROR_SUCCESS)
            return status;
        LINFO("RawDiskDump", "成功连接到烧录内核, 开始下载数据");
        status = fh.DumpDiskContents(start, num, oFile, 0, szPartName);
    } else {
        dw.InitDiskList();
        status = dw.OpenDevice(dnum);
        if (status == ERROR_SUCCESS) {
            LINFO("RawDiskDump", "成功打开设备\n");
            status = dw.DumpDiskContents(start, num, oFile, 0, szPartName);
        }
        dw.CloseDevice();
    }
    return status;
}

int DiskList() {
    DiskWriter dw;
    dw.InitDiskList();

    return ERROR_SUCCESS;
}



int emmcdl_main(int argc, char* argv[]) {
    int dnum = -1;
    int status = 0;
    bool bEmergdl = FALSE;
    char* szOutputFile = NULL;
    char* szXMLFile[MAX_XML_CNT] = { NULL };
    char** szSerialData = { NULL };
    int nSerialDataLen = 0;
    DWORD dwXMLCount = 0;
    char* szFFUImage = NULL;
    char* szFlashProg = NULL;
    char* szSingleImage = NULL;
    char* szPartName = NULL;
    emmc_cmd_e cmd = EMMC_CMD_NONE;
    uint64_t uiStartSector = 0;
    uint64_t uiNumSectors = 0;
    uint64_t uiOffset = 0x40000000;
    DWORD dwGPP1 = 0, dwGPP2 = 0, dwGPP3 = 0, dwGPP4 = 0;
    bool bGppQuiet = FALSE;

    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    LINFO("emmcdl_main", "emmcdl 版本 %d.%d\n", VERSION_MAJOR, VERSION_MINOR);

    for (int i = 0; i < argc; i++)
        LDEBUG("emmcdl_main", "参数 %d: \"%s\"\n", i, argv[i]);

    if (argc < 2) {
        LERROR("emmcdl_main", "参数太少, 无法执行操作");
        return PrintHelp();
    }

    for (int i = 1; i < argc; i++) {
        if (_stricmp(argv[i], "-l") == 0) {
            LINFO("emmcdl_main", "开始列出所有可用的存储设备");
            DiskWriter dw;
            dw.InitDiskList(false);
        }

        if (_stricmp(argv[i], "-lv") == 0) {
            LINFO("emmcdl_main", "开始列出所有可用的存储设备 (详细模式)");
            DiskWriter dw;
            dw.InitDiskList(true);
        }

        if (_stricmp(argv[i], "-r") == 0) {
            LINFO("emmcdl_main", "将命令更新为重置设备");
            cmd = EMMC_CMD_RESET;
        }

        if (_stricmp(argv[i], "-o") == 0) {
            LINFO("emmcdl_main", "设置输出文件名: \"%s\"", argv[i + 1]);
            szOutputFile = argv[++i];
        }

        if (_stricmp(argv[i], "-disk_sector_size") == 0) {
            m_sector_size = atoi(argv[++i]);
            LINFO("emmcdl_main", "设置磁盘扇区大小: %d", m_sector_size);
        }

        
        if (_stricmp(argv[i], "-d") == 0) {
            cmd = EMMC_CMD_DUMP;
            if (isdigit(argv[i + 1][0])) {
                uiStartSector = atoi(argv[++i]);
                uiNumSectors = atoi(argv[++i]);
                LINFO("emmcdl_main", "命令设置为转储数据, 扇区起始位置: %lld, 数量: %lld", uiStartSector, uiNumSectors);
            } else {
                szPartName = argv[++i];
                LINFO("emmcdl_main", "命令设置为转储数据, 分区名称: \"%s\"", szPartName);
            }
        }

        if (_stricmp(argv[i], "-e") == 0) {
            cmd = EMMC_CMD_ERASE;
            LINFO("emmcdl_main", "将命令设置为擦除数据");
            if (isdigit(argv[i + 1][0])) {
                uiStartSector = atoi(argv[++i]);
                uiNumSectors = atoi(argv[++i]);
                LINFO("emmcdl_main", "命令设置为擦除数据, 扇区起始位置: %lld, 数量: %lld", uiStartSector, uiNumSectors);
            } else {
                szPartName = argv[++i];
                LINFO("emmcdl_main", "命令设置为擦除数据, 分区名称: \"%s\"", szPartName);
            }
        }

        if (_stricmp(argv[i], "-w") == 0) {
            LINFO("emmcdl_main", "将命令设置为擦除磁盘布局 (MBR, 主GPT和备份GPT)");
            cmd = EMMC_CMD_WIPE;
        }

        if (_stricmp(argv[i], "-x") == 0) {
            cmd = EMMC_CMD_WRITE;
            szXMLFile[dwXMLCount++] = argv[++i];
            LINFO("emmcdl_main", "将命令设置为写入XML程序文件: \"%s\" (%d/%d)", 
                szXMLFile[dwXMLCount - 1],
                dwXMLCount, MAX_XML_CNT
            );
        }

        if (_stricmp(argv[i], "-p") == 0) {
            if (_strnicmp(argv[i + 1], "COM", 3) == 0) {
                dnum = atoi((argv[++i] + 3));
                LINFO("emmcdl_main", "将端口设置为COM%d", dnum);
            } else {
                dnum = atoi(argv[++i]);
                LINFO("emmcdl_main", "将端口设置为COM%d", dnum);
            }
        }

        if (_stricmp(argv[i], "-s") == 0) {
            uiNumSectors = atoi(argv[++i]);
            LINFO("emmcdl_main", "设置镜像的扇区数: %lld", uiNumSectors);
        }

        if (_stricmp(argv[i], "-f") == 0) {
            szFlashProg = argv[++i];
            bEmergdl = true;
            LINFO("emmcdl_main", "设置烧录内核文件: \"%s\"", szFlashProg);
        }

        if (_stricmp(argv[i], "-i") == 0) {
            cmd = EMMC_CMD_WRITE;
            szSingleImage = argv[++i];
            bEmergdl = true;
            LINFO("emmcdl_main", "将命令设置为写入单个镜像文件: \"%s\"", szSingleImage);
        }

        if (_stricmp(argv[i], "-t") == 0) {
            cmd = EMMC_CMD_TEST;
            if (i < argc) {
                uiOffset = (uint64_t) (atoi(argv[++i])) * 512;
            }
            LINFO("emmcdl_main", "将命令设置为性能测试, 偏移: 0x%llx", uiOffset);
        }

        if (_stricmp(argv[i], "-g") == 0) {
            if ((i + 4) < argc) {
                cmd = EMMC_CMD_GPP;
                dwGPP1 = atoi(argv[++i]);
                dwGPP2 = atoi(argv[++i]);
                dwGPP3 = atoi(argv[++i]);
                dwGPP4 = atoi(argv[++i]);
                LINFO("emmcdl_main", "将命令设置为创建GPP, 大小: %d, %d, %d, %d", dwGPP1, dwGPP2, dwGPP3, dwGPP4);
            } else {
                LERROR("emmcdl_main", "创建GPP指令的参数不足");
                PrintHelp();
            }
        }

        if (_stricmp(argv[i], "-gq") == 0) {
            bGppQuiet = TRUE;
            LINFO("emmcdl_main", "设置为创建GPP时不提示");
        }

        if (_stricmp(argv[i], "-b") == 0) {
            if ((i + 2) < argc) {
                cmd = EMMC_CMD_WRITE_GPT;
                szPartName = argv[++i];
                szSingleImage = argv[++i];
                LINFO("emmcdl_main", "将命令设置为写入数据, 分区: \"%s\", 文件: \"%s\"", 
                    szPartName,
                    szSingleImage
                );
            } else {
                LERROR("emmcdl_main", "写入指令的参数不足");
                PrintHelp();
            }
        }

        if (_stricmp(argv[i], "-gpt") == 0) {
            cmd = EMMC_CMD_GPT;
            LINFO("emmcdl_main", "将命令设置为读取分区表信息");
        }

        if (_stricmp(argv[i], "-info") == 0) {
            cmd = EMMC_CMD_INFO;
            LINFO("emmcdl_main", "将命令设置为读取设备信息");
        }

        if (_stricmp(argv[i], "-ffu") == 0) {
            if ((i + 1) < argc) {
                szFFUImage = argv[++i];
                cmd = EMMC_CMD_LOAD_FFU;
                LINFO("emmcdl_main", "将命令设置为加载FFU文件: \"%s\"", szFFUImage);
            } else {
                LERROR("emmcdl_main", "加载FFU指令的参数不足");
                PrintHelp();
            }
        }

        if (_stricmp(argv[i], "-dumpffu") == 0) {
            if ((i + 1) < argc) {
                szFFUImage = argv[++i];
                szPartName = argv[++i];
                cmd = EMMC_CMD_FFU;
                LINFO("emmcdl_main", "将命令设置为转储FFU文件: \"%s\", 分区: \"%s\"", 
                    szFFUImage,
                    szPartName
                );
            } else {
                LERROR("emmcdl_main", "转储FFU指令的参数不足");
                PrintHelp();
            }
        }

        if (_stricmp(argv[i], "-raw") == 0) {
            if ((i + 1) < argc) {
                szSerialData = &argv[i + 1];
                cmd = EMMC_CMD_RAW;
                LINFO("emmcdl_main", "将命令设置为发送原始数据, 数据如下：");
                for (int j = 0; ; j++) {
                    string line;
                    bool exit_loop = false;
                    for (int k = 0; k < 8; k++) {
                        if (szSerialData[j * 8 + k] == nullptr) {
                            exit_loop = true;
                            break;
                        } else {
                            int val = 0;
                            int stat = sscanf_s(szSerialData[j * 8 + k], "%i", &val);
                            if (stat != 1) {
                                
                            }
                            string byte_str = fmt::format("0x{:02X}", (BYTE)val);
                            line += byte_str;
                            if (k != 15) line += " ";
                            nSerialDataLen++;
                        }
                    }
                    string label = fmt::format("  [0x{:04X}]   ", j * 8);
                    line = label + line;
                    if (line.length() > 0) 
                        LINFO("emmcdl_main", "%s", line.c_str());
                    if (exit_loop) break;
                }
                LINFO("emmcdl_main", "(共%d字节)", nSerialDataLen);
                
            } else {
                LERROR("emmcdl_main", "发送原始数据指令的参数不足 (未指定数据)");
                PrintHelp();
            }
            break;
        }

        if (_stricmp(argv[i], "-splitffu") == 0) {
            if ((i + 1) < argc) {
                szFFUImage = argv[++i];
                cmd = EMMC_CMD_SPLIT_FFU;
                LINFO("emmcdl_main", "将命令设置为分割FFU文件: \"%s\"", szFFUImage);
            } else {
                LERROR("emmcdl_main", "分割FFU指令的参数不足");
                PrintHelp();
            }
        }

        if (_stricmp(argv[i], "-protocol") == 0) {
            if ((i + 1) < argc) {
                if (strcmp(argv[i + 1], "STREAMING") == 0) {
                    m_protocol = STREAMING_PROTOCOL;
                    LINFO("emmcdl_main", "设置协议为STREAMING");
                } else if (strcmp(argv[i + 1], "FIREHOSE") == 0) {
                    m_protocol = FIREHOSE_PROTOCOL;
                    LINFO("emmcdl_main", "设置协议为FIREHOSE");
                }
            } else {
                LERROR("emmcdl_main", "设置协议指令的参数不足 (未指定协议)");
                PrintHelp();
            }
        }

        if (_stricmp(argv[i], "-chipset") == 0) {
            if ((i + 1) < argc) {
                if (_stricmp(argv[i + 1], "8960") == 0) {
                    m_chipset = 8960;
                    LINFO("emmcdl_main", "设置芯片类型为8960");
                } else if (_stricmp(argv[i + 1], "8974") == 0) {
                    m_chipset = 8974;
                    LINFO("emmcdl_main", "设置芯片类型为8974");
                }
            } else {
                LERROR("emmcdl_main", "设置芯片类型指令的参数不足 (未指定芯片类型)");
                PrintHelp();
            }
        }

        if (_stricmp(argv[i], "-MaxPayloadSizeToTargetInBytes") == 0) {
            if ((i + 1) < argc) {
                m_cfg.MaxPayloadSizeToTargetInBytes = atoi(argv[++i]);
                LINFO("emmcdl_main", "设置最大传输单元大小为%dByte(s)", m_cfg.MaxPayloadSizeToTargetInBytes);
            } else {
                LERROR("emmcdl_main", "最大传输单元指令的参数不足 (未指定大小)");
                PrintHelp();
            }
        }

        if (_stricmp(argv[i], "-SkipWrite") == 0) {
            LINFO("emmcdl_main", "设置为跳过写入数据");
            m_cfg.SkipWrite = true;
        }

        if (_stricmp(argv[i], "-SkipStorageInit") == 0) {
            LINFO("emmcdl_main", "设置为跳过存储初始化");
            m_cfg.SkipStorageInit = true;
        }

        if (_stricmp(argv[i], "-SetActivePartition") == 0) {
            if ((i + 1) < argc) {
                m_cfg.ActivePartition = atoi(argv[++i]);
                LINFO("emmcdl_main", "设置活动分区为%d", m_cfg.ActivePartition);
            } else {
                LERROR("emmcdl_main", "设置活动分区指令的参数不足 (未指定分区)");
                PrintHelp();
            }
        }

        if (_stricmp(argv[i], "-MemoryName") == 0) {
            if ((i + 1) < argc) {
                i++;
                if (_stricmp(argv[i], "emmc") == 0) {
                    strcpy_s(m_cfg.MemoryName, sizeof(m_cfg.MemoryName), "emmc");
                    LINFO("emmcdl_main", "设置存储类型为eMMC");
                } else if (_stricmp(argv[i], "ufs") == 0) {
                    strcpy_s(m_cfg.MemoryName, sizeof(m_cfg.MemoryName), "ufs");
                    LINFO("emmcdl_main", "设置存储类型为UFS");
                }
            } else {
                LERROR("emmcdl_main", "存储类型指令的参数不足 (未指定类型)");
                PrintHelp();
            }
        }
    }

    if (szFlashProg != NULL) {
        LINFO("emmcdl_main", "当前指定了烧录内核，尝试写入烧录内核");
        LINFO("emmcdl_main", "尝试打开端口COM%d", dnum);
        int open_res = m_port.Open(dnum);
        if (open_res != ERROR_SUCCESS) {
            LWARN("emmcdl_main", fmt::format("端口COM{}打开失败, 状态: {}", 
                dnum, getErrorDescription(open_res)));
        } else {
            LINFO("emmcdl_main", "端口COM%d打开成功", dnum);
        }

        LINFO("emmcdl_main", "尝试加载烧录内核", dnum);
        status = LoadFlashProg(szFlashProg);
        if (status == ERROR_SUCCESS) {
            LINFO("emmcdl_main", "等待烧录内核启动");
            Sleep(2000);
        } else {
            LWARN("emmcdl_main", fmt::format("烧录内核加载失败, 状态: {}", getErrorDescription(status)));
            LWARN("emmcdl_main", "!!!!!!!! 警告: 烧录内核加载失败, 将会尝试继续执行 !!!!!!!!");
        }
        m_emergency = true;
    }

    switch (cmd) {

        case EMMC_CMD_DUMP:
            if (szOutputFile && (dnum >= 0)) {
                LINFO("emmcdl_main", "将数据转储到文件 \"%s\"", szOutputFile);
                status = RawDiskDump(uiStartSector, uiNumSectors, szOutputFile, dnum, szPartName);
            } else {
                LERROR("emmcdl_main", "转储指令缺少必要参数 (输出文件或端口未指定)");
                return PrintHelp();
            }
            break;

        case EMMC_CMD_ERASE:
            LINFO("EraseDisk", "正在擦除磁盘");
            status = EraseDisk(uiStartSector, uiNumSectors, dnum, szPartName);
            break;

        case EMMC_CMD_SPLIT_FFU:
            status = FFURawProgram(szFFUImage, szOutputFile);
            break;

        case EMMC_CMD_FFU:
            status = FFULoad(szFFUImage, szPartName, szOutputFile);
            break;

        case EMMC_CMD_LOAD_FFU:
            status = FFUProgram(szFFUImage);
            break;

        case EMMC_CMD_WRITE:
            if ((szXMLFile[0] != NULL) && (szSingleImage != NULL)) {
                LERROR("emmcdl_main", "写入指令参数冲突 (不能同时指定XML文件和镜像文件)");
                return PrintHelp();
            }
            if (m_emergency) {
                LINFO("emmcdl_main", "当前指定了烧录内核，尝试写入数据");
                status = EDownloadProgram(szSingleImage, szXMLFile);
            } else {
                LINFO("emmcdl_main", "当前未指定烧录内核，尝试写入数据到磁盘");
                status = RawDiskProgram(szXMLFile, szOutputFile, dnum);
            }
            break;

        case EMMC_CMD_WRITE_GPT:
            if (m_emergency) {
                LINFO("emmcdl_main", "当前指定了烧录内核，尝试写入GPT");
                status = WriteGPT(dnum, szPartName, szSingleImage);
            } else {
                LINFO("emmcdl_main", "当前未指定烧录内核，尝试写入GPT到磁盘");
                status = WriteGPT(dnum, szPartName, szSingleImage);
            }
            break;

        case EMMC_CMD_GPP:
            if (bEmergdl) {
                LINFO("emmcdl_main", "当前指定了烧录内核，尝试创建GPP分区");
                status = CreateGPP(dwGPP1, dwGPP2, dwGPP3, dwGPP4);
            } else {
                LERROR("emmcdl_main", "创建GPP分区需要指定烧录内核 (-f参数)");
                return PrintHelp();
            }
            break;

        case EMMC_CMD_GPT:
            if (m_emergency) {
                LINFO("emmcdl_main", "当前指定了烧录内核，尝试读取GPT");
                status = ReadGPT(dnum);
            } else {
                LINFO("emmcdl_main", "当前未指定烧录内核，尝试从磁盘读取GPT");
                status = ReadGPT(dnum);
            }
            break;

        case EMMC_CMD_INFO:
            if (m_emergency) {
                LINFO("emmcdl_main", "当前指定了烧录内核，尝试读取设备信息");
                status = DumpDeviceInfo();
            } else {
                LERROR("emmcdl_main", "读取设备信息需要指定烧录内核 (-f参数)");
                return PrintHelp();
            }
            break;

        case EMMC_CMD_WIPE:
            if (m_emergency) {
                LINFO("emmcdl_main", "当前指定了烧录内核，尝试擦除磁盘布局");
                status = WipeDisk(dnum);
            } else {
                LINFO("emmcdl_main", "当前未指定烧录内核，尝试擦除磁盘布局");
                status = WipeDisk(dnum);
            }
            break;

        case EMMC_CMD_TEST:
            if (m_emergency) {
                LINFO("emmcdl_main", "当前指定了烧录内核，尝试运行性能测试");
                status = RawDiskTest(dnum, uiOffset);
            } else {
                LINFO("emmcdl_main", "当前未指定烧录内核，尝试运行性能测试");
                status = RawDiskTest(dnum, uiOffset);
            }
            break;

        case EMMC_CMD_RESET:
            LINFO("emmcdl_main", "尝试重置设备");
            status = ResetDevice();
            break;

        case EMMC_CMD_RAW:
            if (m_emergency) {
                LINFO("emmcdl_main", "当前指定了烧录内核，尝试发送原始数据");
                status = RawSerialSend(dnum, szSerialData, nSerialDataLen);
            } else {
                LERROR("emmcdl_main", "发送原始数据需要指定烧录内核 (-f参数)");
                return PrintHelp();
            }
            break;

        default:
            LINFO("emmcdl_main", "未指定任何命令");
            break;
    }

    return status;
}
