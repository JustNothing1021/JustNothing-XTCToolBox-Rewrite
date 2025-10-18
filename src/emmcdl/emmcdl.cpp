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

#include "emmcdl/utils.h"
#include "emmcdl/targetver.h"
#include "emmcdl/emmcdl.h"
#include "emmcdl/partition.h"
#include "emmcdl/diskwriter.h"
#include "emmcdl/dload.h"
#include "emmcdl/sahara.h"
#include "emmcdl/serialport.h"
#include "emmcdl/firehose.h"
#include "emmcdl/ffu.h"
#include "utils/logger.h"
#include "utils/string_utils.h"
#include "tchar.h"
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
    wprintf(L"Usage: emmcdl <option> <value>\n");
    wprintf(L"       Options:\n");
    wprintf(L"       -l                             List available mass storage devices\n");
    wprintf(L"       -info                          List HW information about device attached to COM (eg -p COM8 -info)\n");
    wprintf(L"       -MaxPayloadSizeToTargetInBytes The max bytes in firehose mode (DDR or large IMEM use 16384, default=8192)\n");
    wprintf(L"       -SkipWrite                     Do not write actual data to disk (use this for UFS provisioning)\n");
    wprintf(L"       -SkipStorageInit               Do not initialize storage device (use this for UFS provisioning)\n");
    wprintf(L"       -MemoryName <ufs/emmc>         Memory type default to emmc if none is specified\n");
    wprintf(L"       -SetActivePartition <num>      Set the specified partition active for booting\n");
    wprintf(L"       -disk_sector_size <int>        Dump from start sector to end sector to file\n");
    wprintf(L"       -d <start> <end>               Dump from start sector to end sector to file\n");
    wprintf(L"       -d <PartName>                  Dump entire partition based on partition name\n");
    wprintf(L"       -e <start> <num>               Erase disk from start sector for number of sectors\n");
    wprintf(L"       -e <PartName>                  Erase the entire partition specified\n");
    wprintf(L"       -s <sectors>                   Number of sectors in disk image\n");
    wprintf(L"       -p <port or disk>              Port or disk to program to (eg COM8, for PhysicalDrive1 use 1)\n");
    wprintf(L"       -o <filename>                  Output filename\n");
    wprintf(L"       -x <*.xml>                     Program XML file to output type -o (output) -p (port or disk)\n");
    wprintf(L"       -f <flash programmer>          Flash programmer to load to IMEM eg MPRG8960.hex\n");
    wprintf(L"       -i <singleimage>               Single image to load at offset 0 eg 8960_msimage.mbn\n");
    wprintf(L"       -t                             Run performance tests\n");
    wprintf(L"       -b <prtname> <binfile>         Write <binfile> to GPT <prtname>\n");
    wprintf(L"       -g GPP1 GPP2 GPP3 GPP4         Create GPP partitions with sizes in MB\n");
    wprintf(L"       -gq                            Do not prompt when creating GPP (quiet)\n");
    wprintf(L"       -r                             Reset device\n");
    wprintf(L"       -ffu <*.ffu>                   Download FFU image to device in emergency download need -o and -p\n");
    wprintf(L"       -splitffu <*.ffu> -o <xmlfile> Split FFU into binary chunks and create rawprogram0.xml to output location\n");
    wprintf(L"       -protocol <protocol>           Can be FIREHOSE, STREAMING default is FIREHOSE\n");
    wprintf(L"       -chipset <chipset>             Can be 8960 or 8974 familes\n");
    wprintf(L"       -gpt                           Dump the GPT from the connected device\n");
    wprintf(L"       -raw                           Send and receive RAW data to serial port 0x75 0x25 0x10\n");
    wprintf(L"       -verbose                       Enable verbose output\n");
    wprintf(L"\n\n\nExamples:");
    wprintf(L" emmcdl -p COM8 -info\n");
    wprintf(L" emmcdl -p COM8 -gpt\n");
    wprintf(L" emmcdl -p COM8 -SkipWrite -SkipStorageInit -MemoryName ufs -f prog_emmc_firehose_8994_lite.mbn -x memory_configure.xml\n");
    wprintf(L" emmcdl -p COM8 -f prog_emmc_firehose_8994_lite.mbn -x rawprogram0.xml -SetActivePartition 0\n");
    wprintf(L" emmcdl -p COM8 -f prog_emmc_firehose_8994_lite.mbn -ffu wp8.ffu\n");
    wprintf(L" emmcdl -p COM8 -f prog_emmc_firehose_8994_lite.mbn -d 0 1000 -o dump_1_1000.bin\n");
    wprintf(L" emmcdl -p COM8 -f prog_emmc_firehose_8994_lite.mbn -d SVRawDump -o svrawdump.bin\n");
    wprintf(L" emmcdl -p COM8 -f prog_emmc_firehose_8994_lite.mbn -b SBL1 c:\\temp\\sbl1.mbn\n");
    wprintf(L" emmcdl -p COM8 -f prog_emmc_firehose_8994_lite.mbn -e 0 100\n");
    wprintf(L" emmcdl -p COM8 -f prog_emmc_firehose_8994_lite.mbn -e MODEM_FSG\n");
    wprintf(L" emmcdl -p COM8 -f prog_emmc_firehose_8994_lite.mbn -raw 0x75 0x25 0x10\n");
    return -1;
}

/**
 * @brief               Converts a string to a byte array.
 * @param szSerialData  Original string array
 * @param data          Output byte array
 * @param len           Length of data to convert
 */
void StringToByte(TCHAR** szSerialData, BYTE* data, int len) {
    for (int i = 0; i < len; i++) {
        int val = 0;
        if (szSerialData[i] == nullptr)
            break;
        swscanf_s(szSerialData[i], L"%i", &val);
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
int RawSerialSend(int dnum, TCHAR** szSerialData, int len) {
    int status = ERROR_SUCCESS;
    BYTE data[MAX_RAW_DATA_LEN];

    // Make sure the number of bytes of data we are trying to send is valid
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
        // cout << endl;
        fmt::print(log_utils::Logger::TRACE.message_display_color, "  (无法显示数据)\n");
    }
    status = m_port.Write(data, len);
    BYTE rsp[2048];
    DWORD rsp_len;
    LTRACE("RawSerialSend", "等待响应...");
    status = m_port.Read(rsp, &rsp_len);
    LTRACE("RawSerialSend", "等待响应完成，状态: %s", 
            string_utils::wstr2str(getErrorDescription(status)).c_str());
    LTRACE("RawSerialSend", "接收到的响应: \n%s", string((char*) rsp, rsp_len).c_str());
    return status;
}

int LoadFlashProg(TCHAR* mprgFile) {
    int status = ERROR_SUCCESS;
    // This is PBL so depends on the chip type

    if (m_chipset == 8974) {
        Sahara sh(&m_port);
        if (status != ERROR_SUCCESS)
            return status;
        status = sh.ConnectToDevice(true, 0);
        if (status != ERROR_SUCCESS)
            return status;
        // wprintf(L"Downloading flash programmer: %ls\n", mprgFile);
        LINFO("LoadFlashProg", "正在下载烧录内核，文件来源: \"%s\"", string_utils::wstr2str(mprgFile).c_str());
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
        // wprintf(L"Downloading flash programmer: %ls\n", mprgFile);
        LINFO("LoadFlashProg", "正在下载烧录内核，文件来源: \"%s\"", string_utils::wstr2str(mprgFile).c_str());
        status = dl.LoadFlashProg(mprgFile);
        if (status != ERROR_SUCCESS)
            return status;
    }
    return status;
}

int EraseDisk(uint64_t start, uint64_t num, int dnum, TCHAR* szPartName) {
    int status = ERROR_SUCCESS;

    if (m_emergency) {
        Firehose fh(&m_port);
        fh.SetDiskSectorSize(m_sector_size);
        if (m_verbose)
            fh.EnableVerbose();
        status = fh.ConnectToFlashProg(&m_cfg);
        if (status != ERROR_SUCCESS)
            return status;
        // wprintf(L"Connected to flash programmer, starting download\n");
        LINFO("EraseDisk", "连接到烧录内核，开始擦除磁盘");
        fh.WipeDiskContents(start, num, szPartName);
    } else {
        DiskWriter dw;
        // Initialize and print disk list
        dw.InitDiskList(false);

        status = dw.OpenDevice(dnum);
        if (status == ERROR_SUCCESS) {
            wprintf(L"Successfully opened volume\n");
            wprintf(L"Erase at start_sector %lld: num_sectors: %lld\n", start, num);
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
            wprintf(L"SerialNumber: 0x%08x\n", pbl_info.serial);
            wprintf(L"MSM_HW_ID: 0x%08x\n", pbl_info.msm_id);
            wprintf(L"OEM_PK_HASH: 0x");
            for (int i = 0; i < sizeof(pbl_info.pk_hash); i++) {
                wprintf(L"%02x", pbl_info.pk_hash[i]);
            }
            wprintf(L"\nSBL SW Version: 0x%08x\n", pbl_info.pbl_sw);
        }
    } else {
        wprintf(L"Only devices with Sahara support this information\n");
        status = ERROR_INVALID_PARAMETER;
    }

    return status;
}

// Wipe out existing MBR, Primary GPT and secondary GPT
int WipeDisk(int dnum) {
    DiskWriter dw;
    int status;

    // Initialize and print disk list
    dw.InitDiskList();

    status = dw.OpenDevice(dnum);
    if (status == ERROR_SUCCESS) {
        wprintf(L"Successfully opened volume\n");
        wprintf(L"Wipping GPT and MBR\n");
        status = dw.WipeLayout();
    }
    dw.CloseDevice();
    return status;
}

int CreateGPP(DWORD dwGPP1, DWORD dwGPP2, DWORD dwGPP3, DWORD dwGPP4) {
    int status = ERROR_SUCCESS;

    if (m_protocol == STREAMING_PROTOCOL) {
        Dload dl(&m_port);

        // Wait for device to re-enumerate with flashprg
        status = dl.ConnectToFlashProg(4);
        if (status != ERROR_SUCCESS)
            return status;
        status = dl.OpenPartition(PRTN_EMMCUSER);
        if (status != ERROR_SUCCESS)
            return status;
        wprintf(L"Connected to flash programmer, creating GPP\n");
        status = dl.CreateGPP(dwGPP1, dwGPP2, dwGPP3, dwGPP4);
    } else if (m_protocol == FIREHOSE_PROTOCOL) {
        Firehose fh(&m_port);
        fh.SetDiskSectorSize(m_sector_size);
        if (m_verbose)
            fh.EnableVerbose();
        status = fh.ConnectToFlashProg(&m_cfg);
        if (status != ERROR_SUCCESS)
            return status;
        wprintf(L"Connected to flash programmer, creating GPP\n");
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
        // wprintf(L"Connected to flash programmer, starting download\n");
        wprintf(L"连接到烧录内核，开始读取分区表\n");
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

int WriteGPT(int dnum, TCHAR* szPartName, TCHAR* szBinFile) {
    int status;

    if (m_emergency) {
        Firehose fh(&m_port);
        fh.SetDiskSectorSize(m_sector_size);
        if (m_verbose)
            fh.EnableVerbose();
        status = fh.ConnectToFlashProg(&m_cfg);
        if (status != ERROR_SUCCESS)
            return status;
        // wprintf(L"Connected to flash programmer, starting download\n");
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

int FFUProgram(TCHAR* szFFUFile) {
    FFUImage ffu;
    int status = ERROR_SUCCESS;
    Firehose fh(&m_port);
    fh.SetDiskSectorSize(m_sector_size);
    status = fh.ConnectToFlashProg(&m_cfg);
    if (status != ERROR_SUCCESS)
        return status;
    wprintf(L"Trying to open FFU\n");
    status = ffu.PreLoadImage(szFFUFile);
    if (status != ERROR_SUCCESS)
        return status;
    wprintf(L"Valid FFU found trying to program image\n");
    status = ffu.ProgramImage(&fh, 0);
    ffu.CloseFFUFile();
    return status;
}

int FFULoad(TCHAR* szFFUFile, TCHAR* szPartName, TCHAR* szOutputFile) {
    int status = ERROR_SUCCESS;
    wprintf(_T("Loading FFU\n"));
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

int FFURawProgram(TCHAR* szFFUFile, TCHAR* szOutputFile) {
    int status = ERROR_SUCCESS;
    wprintf(_T("Creating rawprogram and files\n"));
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

int EDownloadProgram(TCHAR* szSingleImage, TCHAR** szXMLFile) {
    int status = ERROR_SUCCESS;
    Dload dl(&m_port);
    Firehose fh(&m_port);
    BYTE prtn = 0;

    if (szSingleImage != NULL) {
        // Wait for device to re-enumerate with flashprg
        status = dl.ConnectToFlashProg(2);
        if (status != ERROR_SUCCESS)
            return status;
        // wprintf(L"Connected to flash programmer, starting download\n");
        LINFO("EDownloadProgram", "连接到烧录内核，开始下载");
        dl.OpenPartition(PRTN_EMMCUSER);
        status = dl.LoadImageFile(szSingleImage);
        dl.ClosePartition();
    } else if (szXMLFile[0] != NULL) {
        // Wait for device to re-enumerate with flashprg
        if (m_protocol == STREAMING_PROTOCOL) {
            status = dl.ConnectToFlashProg(4);
            if (status != ERROR_SUCCESS)
                return status;
            // wprintf(L"Connected to flash programmer, starting download\n");
            LINFO("EDownloadProgram", "连接到烧录内核，开始下载");

            // Download all XML files to device
            for (int i = 0; szXMLFile[i] != NULL; i++) {
                // Use new method to download XML to serial port
                TCHAR szPatchFile[MAX_STRING_LEN];
                wcsncpy_s(szPatchFile, szXMLFile[i], sizeof(szPatchFile));
                StringReplace(szPatchFile, L"rawprogram", L"patch");
                TCHAR* sptr = wcsstr(szXMLFile[i], L".xml");
                if (sptr == NULL)
                    return ERROR_INVALID_PARAMETER;
                prtn = (BYTE) ((*--sptr) - '0' + PRTN_EMMCUSER);
                wprintf(L"Opening partition %d\n", prtn);
                dl.OpenPartition(prtn);
                // Sleep(1);
                status = dl.WriteRawProgramFile(szPatchFile);
                if (status != ERROR_SUCCESS)
                    return status;
                status = dl.WriteRawProgramFile(szXMLFile[i]);
            }
            wprintf(L"Setting Active partition to %d\n", (prtn - PRTN_EMMCUSER));
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
            // wprintf(L"Connected to flash programmer, starting download\n");
            LINFO("EDownloadProgram", "连接到烧录内核，开始下载");

            // Download all XML files to device
            for (int i = 0; szXMLFile[i] != NULL; i++) {
                Partition rawprg(0);
                status = rawprg.PreLoadImage(szXMLFile[i]);
                if (status != ERROR_SUCCESS)
                    return status;
                status = rawprg.ProgramImage(&fh);

                // Only try to do patch if filename has rawprogram in it
                TCHAR* sptr = wcsstr(szXMLFile[i], L"rawprogram");
                if (sptr != NULL && status == ERROR_SUCCESS) {
                    Partition patch(0);
                    int pstatus = ERROR_SUCCESS;
                    // Check if patch file exist
                    TCHAR szPatchFile[MAX_STRING_LEN];
                    wcsncpy_s(szPatchFile, szXMLFile[i], sizeof(szPatchFile));
                    StringReplace(szPatchFile, L"rawprogram", L"patch");
                    pstatus = patch.PreLoadImage(szPatchFile);
                    if (pstatus == ERROR_SUCCESS)
                        patch.ProgramImage(&fh);
                }
            }

            // If we want to set active partition then do that here
            if (m_cfg.ActivePartition >= 0) {
                status = fh.SetActivePartition(m_cfg.ActivePartition);
            }
        }
    }

    return status;
}

int RawDiskProgram(TCHAR** pFile, TCHAR* oFile, uint64_t dnum) {
    DiskWriter dw;
    int status = ERROR_SUCCESS;

    // Depending if we want to write to disk or file get appropriate handle
    if (oFile != NULL) {
        status = dw.OpenDiskFile(oFile, dnum);
    } else {
        int disk = dnum & 0xFFFFFFFF;
        // Initialize and print disk list
        dw.InitDiskList();
        status = dw.OpenDevice(disk);
    }
    if (status == ERROR_SUCCESS) {
        wprintf(L"Successfully opened disk\n");
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

    // Initialize and print disk list
    dw.InitDiskList();
    status = dw.OpenDevice(dnum);
    if (status == ERROR_SUCCESS) {
        wprintf(L"Successfully opened volume\n");
        // status = dw.CorruptionTest(offset);
        status = dw.DiskTest(offset);
    } else {
        wprintf(L"Failed to open volume\n");
    }

    dw.CloseDevice();
    return status;
}

int RawDiskDump(uint64_t start, uint64_t num, TCHAR* oFile, int dnum, TCHAR* szPartName) {
    DiskWriter dw;
    int status = ERROR_SUCCESS;

    // Get extra info from the user via command line
    // wprintf(L"Dumping at start sector: %lld for sectors: %lld to file: %ls\n", start, num, oFile);
    if (szPartName != nullptr) {
        LINFO("RawDiskDump", "开始转储分区 \"%s\" 的数据, 保存到\"%s\"\n", string_utils::wstr2str(szPartName).c_str(), string_utils::wstr2str(oFile).c_str());
    } else {
        LINFO("RawDiskDump", "开始转储数据, 起始扇区=%lld, 扇区总数=%lld, 保存到\"%s\"\n", start, num, string_utils::wstr2str(oFile).c_str());
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
        // wprintf(L"Connected to flash programmer, starting dump\n");
        LINFO("RawDiskDump", "成功连接到烧录内核, 开始下载数据");
        status = fh.DumpDiskContents(start, num, oFile, 0, szPartName);
    } else {
        // Initialize and print disk list
        dw.InitDiskList();
        status = dw.OpenDevice(dnum);
        if (status == ERROR_SUCCESS) {
            // wprintf(L"Successfully opened volume\n");
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



int emmcdl_main(int argc, TCHAR* argv[]) {
    int dnum = -1;
    int status = 0;
    bool bEmergdl = FALSE;
    TCHAR* szOutputFile = NULL;
    TCHAR* szXMLFile[MAX_XML_CNT] = { NULL };
    TCHAR** szSerialData = { NULL };
    int nSerialDataLen = 0;
    DWORD dwXMLCount = 0;
    TCHAR* szFFUImage = NULL;
    TCHAR* szFlashProg = NULL;
    TCHAR* szSingleImage = NULL;
    TCHAR* szPartName = NULL;
    emmc_cmd_e cmd = EMMC_CMD_NONE;
    uint64_t uiStartSector = 0;
    uint64_t uiNumSectors = 0;
    uint64_t uiOffset = 0x40000000;
    DWORD dwGPP1 = 0, dwGPP2 = 0, dwGPP3 = 0, dwGPP4 = 0;
    bool bGppQuiet = FALSE;

    // setmode(fileno(stdout), _O_U8TEXT); // 用来支持wprintf对多字节文字的输出
    // setmode(fileno(stderr), _O_U8TEXT);

    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // wprintf(_T("Version %d.%d\n\n"), VERSION_MAJOR, VERSION_MINOR);
    LINFO("emmcdl_main", "emmcdl 版本 %d.%d\n", VERSION_MAJOR, VERSION_MINOR);

    for (int i = 0; i < argc; i++)
        LDEBUG("emmcdl_main", "参数 %d: \"%s\"\n", i, string_utils::wstr2str(argv[i]).c_str());

    // Print out the version first thing so we know this

    if (argc < 2) {
        LERROR("emmcdl_main", "参数太少, 无法执行操作");
        return PrintHelp();
    }

    // Loop through all our input arguments
    for (int i = 1; i < argc; i++) {
        // Do a list inline

        if (wcsicmp(argv[i], L"-l") == 0) {
            LINFO("emmcdl_main", "开始列出所有可用的存储设备");
            DiskWriter dw;
            dw.InitDiskList(false);
        }

        if (wcsicmp(argv[i], L"-lv") == 0) {
            LINFO("emmcdl_main", "开始列出所有可用的存储设备 (详细模式)");
            DiskWriter dw;
            dw.InitDiskList(true);
        }

        if (wcsicmp(argv[i], L"-r") == 0) {
            LINFO("emmcdl_main", "将命令更新为重置设备");
            cmd = EMMC_CMD_RESET;
        }

        if (wcsicmp(argv[i], L"-o") == 0) {
            // Update command with output filename
            LINFO("emmcdl_main", "设置输出文件名: \"%s\"", string_utils::wstr2str(argv[i + 1]).c_str());
            szOutputFile = argv[++i];
        }

        if (wcsicmp(argv[i], L"-disk_sector_size") == 0) {
            // Update the global disk sector size
            m_sector_size = _wtoi(argv[++i]);
            LINFO("emmcdl_main", "设置磁盘扇区大小: %d", m_sector_size);
        }

        
        if (wcsicmp(argv[i], L"-d") == 0) {
            // Dump from start for number of sectors
            cmd = EMMC_CMD_DUMP;
            // If the next param is alpha then pass in as partition name other wise use sectors
            if (iswdigit(argv[i + 1][0])) {
                uiStartSector = _wtoi(argv[++i]);
                uiNumSectors = _wtoi(argv[++i]);
                LINFO("emmcdl_main", "命令设置为转储数据, 扇区起始位置: %lld, 数量: %lld", uiStartSector, uiNumSectors);
            } else {
                szPartName = argv[++i];
                LINFO("emmcdl_main", "命令设置为转储数据, 分区名称: \"%s\"", string_utils::wstr2str(szPartName).c_str());
            }
        }

        if (wcsicmp(argv[i], L"-e") == 0) {
            cmd = EMMC_CMD_ERASE;
            LINFO("emmcdl_main", "将命令设置为擦除数据");
            // If the next param is alpha then pass in as partition name other wise use sectors
            if (iswdigit(argv[i + 1][0])) {
                uiStartSector = _wtoi(argv[++i]);
                uiNumSectors = _wtoi(argv[++i]);
                LINFO("emmcdl_main", "命令设置为擦除数据, 扇区起始位置: %lld, 数量: %lld", uiStartSector, uiNumSectors);
            } else {
                szPartName = argv[++i];
                LINFO("emmcdl_main", "命令设置为擦除数据, 分区名称: \"%s\"", string_utils::wstr2str(szPartName).c_str());
            }
        }

        if (wcsicmp(argv[i], L"-w") == 0) {
            LINFO("emmcdl_main", "将命令设置为擦除磁盘布局 (MBR, 主GPT和备份GPT)");
            cmd = EMMC_CMD_WIPE;
        }

        if (wcsicmp(argv[i], L"-x") == 0) {
            cmd = EMMC_CMD_WRITE;
            szXMLFile[dwXMLCount++] = argv[++i];
            LINFO("emmcdl_main", "将命令设置为写入XML程序文件: \"%s\" (%d/%d)", 
                string_utils::wstr2str(szXMLFile[dwXMLCount - 1]).c_str(),
                dwXMLCount, MAX_XML_CNT
            );
        }

        if (wcsicmp(argv[i], L"-p") == 0) {
            // Everyone wants to use format COM8 so detect this and accept this as well
            if (_wcsnicmp(argv[i + 1], L"COM", 3) == 0) {
                dnum = _wtoi((argv[++i] + 3));
                LINFO("emmcdl_main", "将端口设置为COM%d", dnum);
            } else {
                dnum = _wtoi(argv[++i]);
                LINFO("emmcdl_main", "将端口设置为COM%d", dnum);
            }
        }

        if (wcsicmp(argv[i], L"-s") == 0) {
            uiNumSectors = _wtoi(argv[++i]);
            LINFO("emmcdl_main", "设置镜像的扇区数: %lld", uiNumSectors);
        }

        if (wcsicmp(argv[i], L"-f") == 0) {
            szFlashProg = argv[++i];
            bEmergdl = true;
            LINFO("emmcdl_main", "设置烧录内核文件: \"%s\"", string_utils::wstr2str(szFlashProg).c_str());
        }

        if (wcsicmp(argv[i], L"-i") == 0) {
            cmd = EMMC_CMD_WRITE;
            szSingleImage = argv[++i];
            bEmergdl = true;
            LINFO("emmcdl_main", "将命令设置为写入单个镜像文件: \"%s\"", string_utils::wstr2str(szSingleImage).c_str());
        }

        if (wcsicmp(argv[i], L"-t") == 0) {
            cmd = EMMC_CMD_TEST;
            if (i < argc) {
                uiOffset = (uint64_t) (_wtoi(argv[++i])) * 512;
            }
            LINFO("emmcdl_main", "将命令设置为性能测试, 偏移: 0x%llx", uiOffset);
        }

        if (wcsicmp(argv[i], L"-g") == 0) {
            if ((i + 4) < argc) {
                cmd = EMMC_CMD_GPP;
                dwGPP1 = _wtoi(argv[++i]);
                dwGPP2 = _wtoi(argv[++i]);
                dwGPP3 = _wtoi(argv[++i]);
                dwGPP4 = _wtoi(argv[++i]);
                LINFO("emmcdl_main", "将命令设置为创建GPP, 大小: %d, %d, %d, %d", dwGPP1, dwGPP2, dwGPP3, dwGPP4);
            } else {
                LERROR("emmcdl_main", "创建GPP指令的参数不足");
                PrintHelp();
            }
        }

        if (wcsicmp(argv[i], L"-gq") == 0) {
            bGppQuiet = TRUE;
            LINFO("emmcdl_main", "设置为创建GPP时不提示");
        }

        if (wcsicmp(argv[i], L"-b") == 0) {
            if ((i + 2) < argc) {
                cmd = EMMC_CMD_WRITE_GPT;
                szPartName = argv[++i];
                szSingleImage = argv[++i];
                LINFO("emmcdl_main", "将命令设置为写入数据, 分区: \"%s\", 文件: \"%s\"", 
                    string_utils::wstr2str(szPartName).c_str(),
                    string_utils::wstr2str(szSingleImage).c_str()
                );
            } else {
                LERROR("emmcdl_main", "写入指令的参数不足");
                PrintHelp();
            }
        }

        if (wcsicmp(argv[i], L"-gpt") == 0) {
            cmd = EMMC_CMD_GPT;
            LINFO("emmcdl_main", "将命令设置为读取分区表信息");
        }

        if (wcsicmp(argv[i], L"-info") == 0) {
            cmd = EMMC_CMD_INFO;
            LINFO("emmcdl_main", "将命令设置为读取设备信息");
        }

        if (wcsicmp(argv[i], L"-ffu") == 0) {
            if ((i + 1) < argc) {
                szFFUImage = argv[++i];
                cmd = EMMC_CMD_LOAD_FFU;
                LINFO("emmcdl_main", "将命令设置为加载FFU文件: \"%s\"", string_utils::wstr2str(szFFUImage).c_str());
            } else {
                LERROR("emmcdl_main", "加载FFU指令的参数不足");
                PrintHelp();
            }
        }

        if (wcsicmp(argv[i], L"-dumpffu") == 0) {
            if ((i + 1) < argc) {
                szFFUImage = argv[++i];
                szPartName = argv[++i];
                cmd = EMMC_CMD_FFU;
                LINFO("emmcdl_main", "将命令设置为转储FFU文件: \"%s\", 分区: \"%s\"", 
                    string_utils::wstr2str(szFFUImage).c_str(),
                    string_utils::wstr2str(szPartName).c_str()
                );
            } else {
                LERROR("emmcdl_main", "转储FFU指令的参数不足");
                PrintHelp();
            }
        }

        if (wcsicmp(argv[i], L"-raw") == 0) {
            if ((i + 1) < argc) {
                szSerialData = &argv[i + 1];
                cmd = EMMC_CMD_RAW;
                LINFO("emmcdl_main", "将命令设置为发送原始数据, 数据如下：");
                for (int j = 0; ; j++) {
                    wstring line;
                    bool exit_loop = false;
                    for (int k = 0; k < 8; k++) {
                        if (szSerialData[j * 8 + k] == nullptr) {
                            exit_loop = true;
                            break;
                        } else {
                            int val = 0;
                            int stat = swscanf_s(szSerialData[j * 8 + k], L"%i", &val);
                            if (stat != 1) {
                                
                            }
                            string byte_str = fmt::format("0x{:02X}", (BYTE)val);
                            line += string_utils::str2wstr(byte_str);
                            if (k != 15) line += L" ";
                            nSerialDataLen++;
                        }
                    }
                    string label = fmt::format("  [0x{:04X}]   ", j * 8);
                    line = string_utils::str2wstr(label) + line;
                    if (line.length() > 0) 
                        LINFO("emmcdl_main", "%s", string_utils::wstr2str(line).c_str());
                    if (exit_loop) break;
                }
                LINFO("emmcdl_main", "(共%d字节)", nSerialDataLen);
                
            } else {
                LERROR("emmcdl_main", "发送原始数据指令的参数不足 (未指定数据)");
                PrintHelp();
            }
            break;
        }

        if (wcsicmp(argv[i], L"-splitffu") == 0) {
            if ((i + 1) < argc) {
                szFFUImage = argv[++i];
                cmd = EMMC_CMD_SPLIT_FFU;
                LINFO("emmcdl_main", "将命令设置为分割FFU文件: \"%s\"", string_utils::wstr2str(szFFUImage).c_str());
            } else {
                LERROR("emmcdl_main", "分割FFU指令的参数不足");
                PrintHelp();
            }
        }

        if (wcsicmp(argv[i], L"-protocol") == 0) {
            if ((i + 1) < argc) {
                if (wcscmp(argv[i + 1], L"STREAMING") == 0) {
                    m_protocol = STREAMING_PROTOCOL;
                    LINFO("emmcdl_main", "设置协议为STREAMING");
                } else if (wcscmp(argv[i + 1], L"FIREHOSE") == 0) {
                    m_protocol = FIREHOSE_PROTOCOL;
                    LINFO("emmcdl_main", "设置协议为FIREHOSE");
                }
            } else {
                LERROR("emmcdl_main", "设置协议指令的参数不足 (未指定协议)");
                PrintHelp();
            }
        }

        if (wcsicmp(argv[i], L"-chipset") == 0) {
            if ((i + 1) < argc) {
                if (wcsicmp(argv[i + 1], L"8960") == 0) {
                    m_chipset = 8960;
                    LINFO("emmcdl_main", "设置芯片类型为8960");
                } else if (wcsicmp(argv[i + 1], L"8974") == 0) {
                    m_chipset = 8974;
                    LINFO("emmcdl_main", "设置芯片类型为8974");
                }
            } else {
                LERROR("emmcdl_main", "设置芯片类型指令的参数不足 (未指定芯片类型)");
                PrintHelp();
            }
        }

        if (wcsicmp(argv[i], L"-MaxPayloadSizeToTargetInBytes") == 0) {
            if ((i + 1) < argc) {
                m_cfg.MaxPayloadSizeToTargetInBytes = _wtoi(argv[++i]);
                LINFO("emmcdl_main", "设置最大传输单元大小为%dByte(s)", m_cfg.MaxPayloadSizeToTargetInBytes);
            } else {
                LERROR("emmcdl_main", "最大传输单元指令的参数不足 (未指定大小)");
                PrintHelp();
            }
        }

        if (wcsicmp(argv[i], L"-SkipWrite") == 0) {
            LINFO("emmcdl_main", "设置为跳过写入数据");
            m_cfg.SkipWrite = true;
        }

        if (wcsicmp(argv[i], L"-SkipStorageInit") == 0) {
            LINFO("emmcdl_main", "设置为跳过存储初始化");
            m_cfg.SkipStorageInit = true;
        }

        if (wcsicmp(argv[i], L"-SetActivePartition") == 0) {
            if ((i + 1) < argc) {
                m_cfg.ActivePartition = _wtoi(argv[++i]);
                LINFO("emmcdl_main", "设置活动分区为%d", m_cfg.ActivePartition);
            } else {
                LERROR("emmcdl_main", "设置活动分区指令的参数不足 (未指定分区)");
                PrintHelp();
            }
        }

        if (wcsicmp(argv[i], L"-MemoryName") == 0) {
            if ((i + 1) < argc) {
                i++;
                if (wcsicmp(argv[i], L"emmc") == 0) {
                    strcpy_s(m_cfg.MemoryName, sizeof(m_cfg.MemoryName), "emmc");
                    LINFO("emmcdl_main", "设置存储类型为eMMC");
                } else if (wcsicmp(argv[i], L"ufs") == 0) {
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
                dnum, string_utils::wstr2str(getErrorDescription(open_res))));
        } else {
            LINFO("emmcdl_main", "端口COM%d打开成功", dnum);
        }

        LINFO("emmcdl_main", "尝试加载烧录内核", dnum);
        status = LoadFlashProg(szFlashProg);
        if (status == ERROR_SUCCESS) {
            // wprintf(_T("Waiting for flash programmer to boot\n"));
            LINFO("emmcdl_main", "等待烧录内核启动");
            Sleep(2000);
        } else {
            LWARN("emmcdl_main", fmt::format("烧录内核加载失败, 状态: {}", string_utils::wstr2str(getErrorDescription(status))));
            // wprintf(_T("\n!!!!!!!! WARNING: Flash programmer failed to load trying to continue !!!!!!!!!\n\n"));
            LWARN("emmcdl_main", "!!!!!!!! 警告: 烧录内核加载失败, 将会尝试继续执行 !!!!!!!!");
        }
        m_emergency = true;
    }

    // If there is a special command execute it
    switch (cmd) {

        case EMMC_CMD_DUMP:
            if (szOutputFile && (dnum >= 0)) {
                // wprintf(_T("Dumping data to file %ls\n"), szOutputFile);
                LINFO("emmcdl_main", "将数据转储到文件 \"%s\"", string_utils::wstr2str(szOutputFile).c_str());
                status = RawDiskDump(uiStartSector, uiNumSectors, szOutputFile, dnum, szPartName);
            } else {
                LERROR("emmcdl_main", "转储指令缺少必要参数 (输出文件或端口未指定)");
                return PrintHelp();
            }
            break;

        case EMMC_CMD_ERASE:
            wprintf(_T("Erasing Disk\n"));
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
                status = EDownloadProgram(szSingleImage, szXMLFile);
            } else {
                wprintf(_T("Programming image\n"));
                status = RawDiskProgram(szXMLFile, szOutputFile, dnum);
            }
            break;

        case EMMC_CMD_WIPE:
            wprintf(_T("Wipping Disk\n"));
            if (dnum > 0) {
                status = WipeDisk(dnum);
            }
            break;

        case EMMC_CMD_RAW:
            // wprintf(_T("Sending RAW data to COM%d\n"), dnum);
            LINFO("emmcdl_main", "向COM%d发送原始数据", dnum);
            status = RawSerialSend(dnum, szSerialData, nSerialDataLen);
            break;

        case EMMC_CMD_TEST:
            wprintf(_T("Running performance tests disk %d\n"), dnum);
            status = RawDiskTest(dnum, uiOffset);
            break;

        case EMMC_CMD_GPP:
            wprintf(_T("Create GPP1=%dMB, GPP2=%dMB, GPP3=%dMB, GPP4=%dMB\n"), (int) dwGPP1, (int) dwGPP2, (int) dwGPP3, (int) dwGPP4);
            if (!bGppQuiet) {
                wprintf(_T("Are you sure? (y/n)"));
                if (getchar() != 'y') {
                    wprintf(_T("\nGood choice back to safety\n"));
                    break;
                }
            }
            wprintf(_T("Sending command to create GPP\n"));
            status = CreateGPP(dwGPP1 * 1024 * 2, dwGPP2 * 1024 * 2, dwGPP3 * 1024 * 2, dwGPP4 * 1024 * 2);
            if (status == ERROR_SUCCESS) {
                wprintf(_T("Power cycle device to complete operation\n"));
            }
            break;

        case EMMC_CMD_WRITE_GPT:
            if ((szSingleImage != NULL) && (szPartName != NULL) && (dnum >= 0)) {
                status = WriteGPT(dnum, szPartName, szSingleImage);
            }
            break;

        case EMMC_CMD_RESET:
            status = ResetDevice();
            break;

        case EMMC_CMD_LOAD_MRPG:
            break;

        case EMMC_CMD_GPT:
            // Read and dump GPT information from given disk
            status = ReadGPT(dnum);
            break;

        case EMMC_CMD_INFO:
            m_port.Open(dnum);
            status = DumpDeviceInfo();
            break;

        case EMMC_CMD_NONE:
            break;
    }

    // Display the error message and exit the process
    if (status != ERROR_SUCCESS) {
        LERROR("emmcdl_main", fmt::format("执行出错, 状态: {}", string_utils::wstr2str(getErrorDescription(status))));
    } else {
        LINFO("emmcdl_main", fmt::format("执行成功, 状态: {}", string_utils::wstr2str(getErrorDescription(status))));
    }
    return status;
}
