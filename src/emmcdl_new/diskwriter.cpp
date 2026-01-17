#include "emmcdl_new/diskwriter.h"
#include "emmcdl_new/sahara.h"
#include "utils/logger.h"
#include "emmcdl_new/utils.h"

// Only List COM port information for desktop version
#ifndef ARM
#include "setupapi.h"
#define INITGUID 1 // This is needed to properly define the GUID's in devpkey.h
#include "devpkey.h"
#endif // ARM

DiskWriter::DiskWriter() {
  ovl.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
  hVolume = INVALID_HANDLE_VALUE;
  bPatchDisk = false;

  // 创建128字节对齐的缓冲区
  disks = nullptr;
  volumes = nullptr;
  gpt_entries = (gpt_entry_t*) malloc(sizeof(gpt_entry_t) * 256);
  disks = (disk_entry_t*) malloc(sizeof(disk_entry_t) * MAX_DISKS);
  volumes = (vol_entry_t*) malloc(sizeof(vol_entry_t) * MAX_VOLUMES);
  
  if (!gpt_entries || !disks || !volumes) {
    LERROR("DiskWriter::DiskWriter", "内存分配失败");
    throw std::bad_alloc();
  }
}

DiskWriter::~DiskWriter() {
  // 释放所有分配的资源
  if (disks) {
    free(disks);
    disks = nullptr;
  }
  if (volumes) {
    free(volumes);
    volumes = nullptr;
  }
  if (gpt_entries) {
    free(gpt_entries);
    gpt_entries = nullptr;
  }
  if (ovl.hEvent) {
    CloseHandle(ovl.hEvent);
    ovl.hEvent = NULL;
  }
}

int DiskWriter::ProgramRawCommand(const std::string& key) {
  (void)key;
  return ERROR_INVALID_FUNCTION;
}

int DiskWriter::DeviceReset(void) {
  return ERROR_INVALID_FUNCTION;
}

int DiskWriter::GetVolumeInfo(vol_entry_t* vol) {
  char mount[MAX_PATH + 1];
  DWORD mountsize;

  memset(mount, 0, sizeof(mount));

  if (GetVolumePathNamesForVolumeNameA(vol->rootpath, mount, MAX_PATH, &mountsize)) {
    DWORD maxcomplen;
    DWORD fsflags;
    strcpy_s(vol->mount, mount);
    vol->mount[strlen(vol->mount) - 1] = 0;
    vol->drivetype = GetDriveTypeA(vol->mount);
    
    if (vol->drivetype == DRIVE_REMOVABLE || vol->drivetype == DRIVE_FIXED) {
      GetVolumeInformationA(vol->mount, vol->volume, MAX_PATH + 1, (LPDWORD) &vol->serialnum, &maxcomplen, &fsflags, vol->fstype, MAX_PATH);
      char volPath[MAX_PATH + 1] = "\\\\.\\";
      strcat_s(volPath, vol->mount);

      hDisk = CreateFileA(volPath,
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);
      if (hDisk != INVALID_HANDLE_VALUE) {
        _STORAGE_DEVICE_NUMBER devInfo;
        DWORD bytesRead;
        DeviceIoControl(hDisk, IOCTL_STORAGE_GET_DEVICE_NUMBER, NULL, 0, &devInfo, sizeof(devInfo), &bytesRead, NULL);
        vol->disknum = devInfo.DeviceNumber;
        CloseHandle(hDisk);
        LDEBUG("DiskWriter::GetVolumeInfo", "卷%s对应的磁盘号: %d", vol->mount, vol->disknum);
      }
    }
  }

  return ERROR_SUCCESS;
}

int DiskWriter::GetDiskInfo(disk_entry_t* de) {
  char tPath[MAX_PATH + 1];
  int status = ERROR_SUCCESS;

  sprintf_s(tPath, "\\\\.\\PhysicalDrive%d", de->disknum);
  
  hDisk = CreateFileA(tPath,
    GENERIC_READ,
    (FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE),
    NULL,
    OPEN_EXISTING,
    FILE_FLAG_OVERLAPPED,
    NULL);
  if (hDisk != INVALID_HANDLE_VALUE) {
    DISK_GEOMETRY_EX info;
    DWORD bytesRead;
    if (DeviceIoControl(hDisk,
      IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,
      NULL,
      0,
      &info,
      sizeof(info),
      &bytesRead,
      &ovl
    )) {
      strcpy_s(de->diskname, tPath);
      de->disksize = *(uint64_t*) (&info.DiskSize);
      de->blocksize = info.Geometry.BytesPerSector;
      LDEBUG("DiskWriter::GetDiskInfo", "磁盘%d信息: 大小=%lld字节, 块大小=%d字节", 
             de->disknum, de->disksize, de->blocksize);
    } else {
      status = ERROR_NO_VOLUME_LABEL;
      LWARN("DiskWriter::GetDiskInfo", "无法获取磁盘%d的几何信息", de->disknum);
    }
    CloseHandle(hDisk);
  } else {
    status = GetLastError();
    LERROR("DiskWriter::GetDiskInfo", "无法打开磁盘%d: %s", 
           de->disknum, getErrorDescription(status).c_str());
  }

  if (status != ERROR_SUCCESS) {
    de->disknum = -1;
    de->disksize = (uint64_t) -1;
    de->diskname[0] = 0;
    de->volnum[0] = -1;
  }

  return ERROR_SUCCESS;
}

int DiskWriter::InitDiskList(bool verbose) {
  HANDLE vHandle;
  char VolumeName[MAX_PATH + 1];
  BOOL bValid = true;
  int i = 0;
#ifndef ARM
  HDEVINFO hDevInfo = SetupDiGetClassDevsA(&GUID_DEVINTERFACE_COMPORT, NULL, NULL, DIGCF_DEVICEINTERFACE | DIGCF_PRESENT);
  DEVPROPTYPE ulPropertyType = DEVPROP_TYPE_STRING;
  DWORD dwSize;
#endif

  if (disks == nullptr || volumes == nullptr) {
    LERROR("DiskWriter::InitDiskList", "磁盘或卷列表未初始化");
    return ERROR_INVALID_PARAMETER;
  }

  LDEBUG("DiskWriter::InitDiskList", "正在查找所有EDL模式的设备...");

#ifndef ARM
  if (hDevInfo != INVALID_HANDLE_VALUE) {
    for (int i = 0;; i++) {
      WCHAR szBuffer[512];
      SP_DEVINFO_DATA DeviceInfoData;
      DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
      if (!SetupDiEnumDeviceInfo(hDevInfo, i, &DeviceInfoData) && (GetLastError() == ERROR_NO_MORE_ITEMS)) {
        break;
      }
      if (SetupDiGetDeviceProperty(hDevInfo, &DeviceInfoData, &DEVPKEY_Device_FriendlyName, &ulPropertyType, (BYTE*) szBuffer, sizeof(szBuffer), &dwSize, 0)) {
        std::string szBufferA(szBuffer, szBuffer + wcslen(szBuffer));
        if ((GetLastError() == ERROR_SUCCESS) && strstr(szBufferA.c_str(), "QDLoader 9008") != nullptr) {
          LDEBUG("DiskWriter::InitDiskList", "发现EDL设备: %s", szBufferA.c_str());
          if (verbose) {
            WCHAR* port = wcsstr(szBuffer, L"COM");
            if (port != nullptr) {
              SerialPort spTemp;
              pbl_info_t pbl_info;
              spTemp.Open(_wtoi((port + 3)));
              Sahara sh(&spTemp);
              int status = sh.DumpDeviceInfo(&pbl_info);
              if (status == ERROR_SUCCESS) {
                LINFO("DiskWriter::InitDiskList", " => 序列号=0x%x 硬件ID=0x%x", pbl_info.serial, pbl_info.msm_id);
              } else {
                LWARN("DiskWriter::InitDiskList", " => 无法读取设备信息: %s", 
                      getErrorDescription(status).c_str());
              }
            }
          }
        }
      }
    }
    SetupDiDestroyDeviceInfoList(hDevInfo);
  } else {
    LWARN("DiskWriter::InitDiskList", "无法获取设备信息列表");
  }
#endif

  memset(volumes, 0, sizeof(vol_entry_t) * MAX_VOLUMES);

  for (i = 0; i < MAX_VOLUMES; i++) {
    volumes[i].disknum = -1;
  }

  LDEBUG("DiskWriter::InitDiskList", "正在查找所有磁盘...");
  vHandle = FindFirstVolumeA(VolumeName, MAX_PATH);
  for (i = 0; bValid && (i < MAX_VOLUMES); i++) {
    strcpy_s(volumes[i].rootpath, VolumeName);
    GetVolumeInfo(&volumes[i]);
    bValid = FindNextVolumeA(vHandle, VolumeName, MAX_PATH);
  }

  for (i = 0; i < MAX_DISKS; i++) {
    disks[i].disknum = i;
    if (GetDiskInfo(&disks[i]) != ERROR_SUCCESS) {
      continue;
    }
    int v = 0;
    for (int j = 0; j < MAX_VOLUMES; j++) {
      if (volumes[j].disknum == i) {
        disks[i].volnum[v++] = j;
      }
    }
    disks[i].volnum[v] = 0;

    if (disks[i].disknum != -1) {
      LINFO("DiskWriter::GetDiskInfo", "%d. %s  大小: %lldMB, (%lld 扇区), 块大小: %d 挂载点:%s, 名称:[%s]",
        i,
        disks[i].diskname,
        disks[i].disksize / (1024 * 1024),
        (disks[i].disksize / (disks[i].blocksize)),
        disks[i].blocksize,
        volumes[disks[i].volnum[0]].mount,
        volumes[disks[i].volnum[0]].volume
      );
    }
  }

  return ERROR_SUCCESS;
}

int DiskWriter::ProgramPatchEntry(PartitionEntry pe, const std::string& key) {
  (void)key;
  DWORD bytesIn, bytesOut;
  BOOL bPatchFile = FALSE;
  int status = ERROR_SUCCESS;

  if ((pe.filename == "DISK") && bPatchDisk) {
    LINFO("DiskWriter::PatchFile", "在磁盘上打补丁");
  } else if ((pe.filename != "DISK") && !bPatchDisk) {
    LINFO("DiskWriter::PatchFile", "在本地打补丁");
    hDisk = CreateFileA(pe.filename.c_str(),
      (GENERIC_READ | GENERIC_WRITE),
      0,
      NULL,
      OPEN_EXISTING,
      FILE_ATTRIBUTE_NORMAL,
      NULL);
    if (hDisk == INVALID_HANDLE_VALUE) {
      LERROR("DiskWriter::PatchFile", "打开文件失败: %s", pe.filename.c_str());
      return GetLastError();
    }
    bPatchFile = TRUE;
  } else {
    LWARN("DiskWriter::PatchFile", "未指定文件，跳过命令");
    return ERROR_SUCCESS;
  }

  if (pe.crc_size > 0) {
    if (ReadData(buffer1, pe.crc_start * DISK_SECTOR_SIZE, ((int) pe.crc_size + DISK_SECTOR_SIZE), &bytesIn, pe.physical_partition_number) == ERROR_SUCCESS) {
      Partition p;
      pe.patch_value += p.CalcCRC32(buffer1, (int) pe.crc_size);
      LDEBUG("DiskWriter::PatchFile", "补丁值: 0x%x", (uint32_t) pe.patch_value);
    }
  }

  if ((status = ReadData(buffer1, pe.start_sector * DISK_SECTOR_SIZE, DISK_SECTOR_SIZE, &bytesIn, 0)) == ERROR_SUCCESS) {
    memcpy_s(&(buffer1[pe.patch_offset]), (int) (MAX_TRANSFER_SIZE - pe.patch_offset), &(pe.patch_value), (int) pe.patch_size);
    status = WriteData(buffer1, pe.start_sector * DISK_SECTOR_SIZE, DISK_SECTOR_SIZE, &bytesOut, 0);
  }

  if (bPatchFile) {
    CloseHandle(hDisk);
  }
  return status;
}

#define MAX_TEST_SIZE (4 * 1024 * 1024)
#define LOOP_COUNT 1000

int DiskWriter::CorruptionTest(uint64_t offset) {
  int status = ERROR_SUCCESS;
  BOOL bWriteDone = FALSE;
  BOOL bReadDone = FALSE;
  DWORD bytesOut = 0;
  uint64_t ticks;

  char* bufAlloc = NULL;
  char* temp1 = NULL;

  bufAlloc = new char[MAX_TEST_SIZE + 1024 * 1024];

  // Round off to 1MB boundary
  temp1 = (char*) (((uint64_t) bufAlloc + 1024 * 1024) & ~0xFFFFF);
  ticks = GetTickCount64();
  LDEBUG("DiskWriter::TestPerformance", "OffsetLow: 0x%x", (uint32_t) offset);
  LDEBUG("DiskWriter::TestPerformance", "OffsetHigh: 0x%x", (uint32_t) (offset >> 32));
  LDEBUG("DiskWriter::TestPerformance", "temp1: 0x%x", (uint64_t) temp1);

  for (int i = 0; i < MAX_TEST_SIZE; i++) {
    temp1[i] = (char) i;
  }

  for (int i = 0; i < LOOP_COUNT; i++) {
    ovl.OffsetHigh = (offset >> 32);
    ovl.Offset = offset & 0xFFFFFFFF;

    bWriteDone = WriteFile(hDisk, &temp1[i], MAX_TEST_SIZE, NULL, &ovl);
    if (!bWriteDone)
      status = GetLastError();
    if (!bWriteDone && (status != ERROR_SUCCESS)) {
      LERROR("DiskWriter::TestPerformance", "写入失败 - 状态: %d, 写入完成: %d, 写入字节: %d", status, (int) bWriteDone, (int) bytesOut);
      break;
    }

    ovl.Offset = offset & 0xFFFFFFFF;
    ovl.OffsetHigh = (offset >> 32);

    bReadDone = ReadFile(hDisk, &temp1[i], MAX_TEST_SIZE, NULL, &ovl);
    if (!bReadDone)
      status = GetLastError();
    if (!bReadDone && (status != ERROR_SUCCESS)) {
      LERROR("DiskWriter::TestPerformance", "读取失败 - 状态: %d, 读取完成: %d, 读取字节: %d", status, (int) bReadDone, (int) bytesOut);
      break;
    }

    for (int i = 0; i < MAX_TEST_SIZE; i++) {
      if (temp1[i] != (char) i) {
        // If it is different dump out buffer
        LERROR("DiskWriter::TestPerformance", "数据验证失败 - 偏移: %d, 期望: %d, 实际: %d", i, (i & 0xff), (int) temp1[i]);
      }
    }
    LDEBUG("DiskWriter::TestPerformance", ".");
    offset += 0x40;
  }

  delete[] bufAlloc;
  return ERROR_SUCCESS;
}

int DiskWriter::DiskTest(uint64_t offset) {
  int status = ERROR_SUCCESS;
  BOOL bWriteDone = FALSE;
  BOOL bReadDone = FALSE;
  DWORD bytesOut = 0;
  uint64_t ticks;
  DWORD iops;

  char* bufAlloc = NULL;
  char* temp1 = NULL;
  LINFO("DiskWriter::TestPerformance", "开始性能测试");

  bufAlloc = new char[MAX_TEST_SIZE + 1024 * 1024];

  // Round off to 1MB boundary
  temp1 = (char*) (((uint64_t) bufAlloc + 1024 * 1024) & ~0xFFFFF);

  LINFO("DiskWriter::TestPerformance", "顺序写入测试 - 4MB缓冲区: %d", (uint64_t) temp1);
  ovl.Offset = (offset & 0xffffffff);
  ovl.OffsetHigh = (offset >> 32);

  ticks = GetTickCount64();
  for (int i = 0; i < 50; i++) {
    bWriteDone = WriteFile(hDisk, temp1, MAX_TEST_SIZE, &bytesOut, &ovl);
    if (!bWriteDone)
      status = GetLastError();
    if (!bWriteDone && (status != ERROR_SUCCESS)) {
      LERROR("DiskWriter::TestPerformance", "顺序写入失败 - 状态: %d, 写入完成: %d, 输出字节: %d", status, (int) bWriteDone, (int) bytesOut);
      break;
    }
  }
  ticks = GetTickCount64() - ticks;
  LINFO("DiskWriter::TestPerformance", "顺序写入传输速率: %d KB/s", (int) (50 * MAX_TEST_SIZE / ticks));

  ovl.Offset = (offset & 0xffffffff);
  ovl.OffsetHigh = (offset >> 32);

  LINFO("DiskWriter::TestPerformance", "随机写入测试 - 4KB缓冲区");
  ticks = GetTickCount64();
  for (iops = 0; (GetTickCount64() - ticks) < 2000; iops++) {
    bReadDone = WriteFile(hDisk, temp1, 4 * 1024, NULL, &ovl);
    if (!bWriteDone)
      status = GetLastError();
    if (!bWriteDone && (status != ERROR_SUCCESS)) {
      LERROR("DiskWriter::TestPerformance", "随机写入失败 - 状态: %d, 偏移: %d, 写入完成: %d, 写入字节: %d", status, (uint32_t) ovl.Offset, (int) bWriteDone, (int) bytesOut);
      break;
    }
    ovl.Offset += 0x1000;
  }
  ticks = GetTickCount64() - ticks;
  LINFO("DiskWriter::TestPerformance", "随机4K写入IOPS = %d", (int) (iops / 2));

  LINFO("DiskWriter::TestPerformance", "刷新缓冲区!");
  ovl.Offset = (offset & 0xffffffff);
  ovl.OffsetHigh = (offset >> 32);
  ticks = GetTickCount64();
  for (int i = 0; i < 100; i++) {
    bReadDone = ReadFile(hDisk, temp1, MAX_TEST_SIZE, NULL, &ovl);
    if (!bReadDone)
      status = GetLastError();
    if (!bReadDone && (status != ERROR_SUCCESS)) {
      LERROR("DiskWriter::TestPerformance", "读取失败 - 状态: %d, 读取完成: %d, 读取字节: %d", status, (int) bReadDone, (int) bytesOut);
      break;
    }
  }

  LINFO("DiskWriter::TestPerformance", "顺序读取测试 - 4MB缓冲区: %d", (uint64_t) temp1);
  ovl.Offset = (offset & 0xffffffff);
  ovl.OffsetHigh = (offset >> 32);
  ticks = GetTickCount64();
  for (int i = 0; i < 50; i++) {
    bReadDone = ReadFile(hDisk, temp1, MAX_TEST_SIZE, NULL, &ovl);
    if (!bReadDone)
      status = GetLastError();
    if (!bReadDone && (status != ERROR_SUCCESS)) {
      LERROR("DiskWriter::TestPerformance", "顺序读取失败 - 状态: %d, 读取完成: %d, 读取字节: %d", status, (int) bReadDone, (int) bytesOut);
      break;
    }
  }

  ticks = GetTickCount64() - ticks;
  LINFO("DiskWriter::TestPerformance", "顺序读取传输速率: %d KB/s, 耗时: %d ms", (int) (50 * MAX_TEST_SIZE / ticks), (int) ticks);

  LINFO("DiskWriter::TestPerformance", "随机读取测试 - 4KB缓冲区");
  ticks = GetTickCount64();
  for (iops = 0; (GetTickCount64() - ticks) < 2000; iops++) {
    bReadDone = ReadFile(hDisk, temp1, 4 * 1024, NULL, &ovl);
    if (!bReadDone)
      status = GetLastError();
    if (!bReadDone && (status != ERROR_SUCCESS)) {
      LERROR("DiskWriter::TestPerformance", "随机读取失败 - 状态: %d, 读取完成: %d, 读取字节: %d", status, (int) bReadDone, (int) bytesOut);
      break;
    }
    ovl.Offset += 0x100000;
  }
  LINFO("DiskWriter::TestPerformance", "随机4K读取IOPS = %d", (int) (iops / 2));

  delete[] bufAlloc;
  return status;
}

int DiskWriter::WriteData(BYTE* writeBuffer, int64_t writeOffset, DWORD writeBytes, DWORD* bytesWritten, uint8_t partNum) {
  (void)partNum;
  OVERLAPPED ovlWrite;
  BOOL bWriteDone;
  int status = ERROR_SUCCESS;

  // Check inputs
  if (bytesWritten == NULL) {
    return ERROR_INVALID_PARAMETER;
  }

  // If disk handle not opened then return error
  if ((hDisk == NULL) || (hDisk == INVALID_HANDLE_VALUE)) {
    return ERROR_INVALID_HANDLE;
  }

  ovlWrite.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
  if (ovlWrite.hEvent == NULL) {
    return ERROR_INVALID_HANDLE;
  }

  ResetEvent(ovlWrite.hEvent);
  ovlWrite.Offset = (DWORD) (writeOffset);
  ovlWrite.OffsetHigh = ((writeOffset) >> 32);

  // Write the data to the disk/file at the given offset
  bWriteDone = WriteFile(hDisk, writeBuffer, writeBytes, bytesWritten, &ovlWrite);
  if (!bWriteDone)
    status = GetLastError();

  // If not done and IO is pending wait for it to complete
  if (!bWriteDone && (status == ERROR_IO_PENDING)) {
    status = ERROR_SUCCESS;
    bWriteDone = GetOverlappedResult(hDisk, &ovlWrite, bytesWritten, TRUE);
    if (!bWriteDone)
      status = GetLastError();
  }

  CloseHandle(ovlWrite.hEvent);
  return status;
}

int DiskWriter::ReadData(BYTE* readBuffer, int64_t readOffset, DWORD readBytes, DWORD* bytesRead, uint8_t partNum) {
  OVERLAPPED ovlRead;
  BOOL bReadDone = TRUE;
  int status = ERROR_SUCCESS;

  LTRACE("DiskWriter::ReadData", "尝试从分区%d读取数据, 缓存至%p(大小为%d bytes)", partNum, (void*) readBuffer, readBytes);


  // Check input parameters
  if (bytesRead == NULL) {
    return ERROR_INVALID_PARAMETER;
  }

  // Make sure we have a valid handle to our disk/file
  if ((hDisk == NULL) || (hDisk == INVALID_HANDLE_VALUE)) {
    return ERROR_INVALID_HANDLE;
  }

  ovlRead.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
  if (ovlRead.hEvent == NULL) {
    return ERROR_INVALID_HANDLE;
  }

  ResetEvent(ovlRead.hEvent);
  ovlRead.Offset = (DWORD) readOffset;
  ovlRead.OffsetHigh = (readOffset >> 32);

  bReadDone = ReadFile(hDisk, readBuffer, readBytes, bytesRead, &ovlRead);
  if (!bReadDone)
    status = GetLastError();

  // If not done and IO is pending wait for it to complete
  if (!bReadDone && (status == ERROR_IO_PENDING)) {
    status = ERROR_SUCCESS;
    bReadDone = GetOverlappedResult(hDisk, &ovlRead, bytesRead, TRUE);
    if (!bReadDone)
      status = GetLastError();
  }

  CloseHandle(ovlRead.hEvent);
  return status;
}

int DiskWriter::WriteData(const ByteArray& writeBuffer, int64_t writeOffset, DWORD* bytesWritten, uint8_t partNum) {
    return WriteData((BYTE*)writeBuffer.data(), writeOffset, writeBuffer.size(), bytesWritten, partNum);
}

int DiskWriter::ReadData(ByteArray& readBuffer, int64_t readOffset, DWORD readBytes, DWORD* bytesRead, uint8_t partNum) {
    readBuffer.resize(readBytes);
    int status = ReadData(readBuffer.data(), readOffset, readBytes, bytesRead, partNum);
    if (status == ERROR_SUCCESS && *bytesRead < readBytes) {
        readBuffer.resize(*bytesRead);
    }
    return status;
}

int DiskWriter::UnmountVolume(vol_entry_t vol) {
  DWORD bytesRead = 0;

  char volPath[MAX_PATH + 1] = "\\\\.\\";
  strcat_s(volPath, vol.mount);
  hVolume = CreateFileA(volPath,
    (GENERIC_READ | GENERIC_WRITE),
    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
    NULL,
    OPEN_EXISTING,
    FILE_ATTRIBUTE_NORMAL,
    NULL);

  if (hVolume != INVALID_HANDLE_VALUE) {
    DeviceIoControl(hVolume, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &bytesRead, NULL);
    LDEBUG("DiskWriter::UnmountVolume", "成功卸载卷%s", vol.mount);
  } else {
    LWARN("DiskWriter::UnmountVolume", "无法卸载卷%s", volPath);
  }
  return ERROR_SUCCESS;
}

int DiskWriter::OpenDiskFile(const char* oFile, uint64_t sectors) {
  int status = ERROR_SUCCESS;
  if (oFile == NULL) {
    return ERROR_INVALID_HANDLE;
  }

  hDisk = CreateFileA(oFile,
    GENERIC_WRITE | GENERIC_READ,
    0,
    NULL,
    CREATE_NEW,
    FILE_FLAG_OVERLAPPED,
    NULL);
  if (hDisk == INVALID_HANDLE_VALUE) {
    status = GetLastError();
    if (status == ERROR_FILE_EXISTS) {
      status = ERROR_SUCCESS;
      hDisk = CreateFileA(oFile,
        GENERIC_WRITE | GENERIC_READ,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED,
        NULL);
    }
    if (hDisk == INVALID_HANDLE_VALUE) {
      status = GetLastError();
    }
  }
  disk_size = sectors * DISK_SECTOR_SIZE;
  return status;
}

int DiskWriter::OpenDevice(int dn) {
  disk_entry_t de = disks[dn];
  if (de.volnum[0] != 0) {
    if (UnmountVolume(volumes[de.volnum[0]]) != ERROR_SUCCESS) {
      LWARN("DiskWriter::OpenDevice", "无法卸载卷%s", volumes[de.volnum[0]].mount);
    }
  }

  char tPath[MAX_PATH];
  sprintf_s(tPath, "\\\\.\\PhysicalDrive%d", dn);
  LDEBUG("DiskWriter::OpenDevice", "正在使用磁盘 %s\n", tPath);
  hDisk = CreateFileA(tPath,
    GENERIC_WRITE | GENERIC_READ,
    (FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE),
    NULL,
    OPEN_EXISTING,
    FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH | FILE_FLAG_OVERLAPPED,
    NULL);

  disk_num = dn;
  disk_size = disks[dn].disksize;
  return ERROR_SUCCESS;
}

void DiskWriter::CloseDevice() {
  disk_num = -1;
  if (hDisk != INVALID_HANDLE_VALUE)
    CloseHandle(hDisk);
  if (hVolume != INVALID_HANDLE_VALUE)
    CloseHandle(hVolume);
}

bool DiskWriter::IsDeviceWriteable() {
  DWORD bytesRead;
  if (DeviceIoControl(hDisk, IOCTL_DISK_IS_WRITABLE, NULL, 0, NULL, 0, &bytesRead, NULL)) {
    return true;
  }
  return false;
}

int DiskWriter::WipeLayout() {
  DWORD bytesRead;
  if (DeviceIoControl(hDisk, IOCTL_DISK_DELETE_DRIVE_LAYOUT, NULL, 0, NULL, 0, &bytesRead, &ovl)) {

    if (DeviceIoControl(hDisk, IOCTL_DISK_UPDATE_PROPERTIES, NULL, 0, NULL, 0, &bytesRead, &ovl)) {
      return ERROR_SUCCESS;
    }
  }
  return ERROR_GEN_FAILURE;
}

int DiskWriter::RawReadTest(uint64_t offset) {
  // Set up the overlapped structure
  OVERLAPPED ovlp;
  int status = ERROR_SUCCESS;
  LARGE_INTEGER large_val;
  large_val.QuadPart = offset;
  ovlp.hEvent = CreateEvent(NULL, FALSE, FALSE, _T("DiskWriter"));
  if (!ovlp.hEvent)
    return GetLastError();
  ovlp.Offset = large_val.LowPart;
  ovlp.OffsetHigh = large_val.HighPart;

  DWORD bytesIn = 0;
  DWORD dwResult;
  BOOL bResult = ReadFile(hDisk, buffer1, DISK_SECTOR_SIZE, NULL, &ovlp);
  dwResult = GetLastError();
  if (!bResult) {
    // If Io is pending wait for it to finish
    if (dwResult == ERROR_IO_PENDING) {
      // Wait for actual data to be read if pending
      LDEBUG("DiskWriter::ReadData", "等待操作完成 - 偏移: %lld", offset);
      bResult = GetOverlappedResult(hDisk, &ovlp, &bytesIn, TRUE);
      if (!bResult) {
        dwResult = GetLastError();
        if (dwResult != ERROR_IO_INCOMPLETE) {
          status = ERROR_READ_FAULT;
        }
      } else {
        // Finished read successfully
        status = ERROR_SUCCESS;
      }
    } else if (dwResult != ERROR_SUCCESS) {
      LERROR("DiskWriter::ReadData", "操作状态: %d", (int) dwResult);
      status = ERROR_READ_FAULT;
    }
  }

  CloseHandle(ovlp.hEvent);
  return status;
}

int DiskWriter::FastCopy(HANDLE hRead, int64_t sectorRead, HANDLE hWrite, int64_t sectorWrite, uint64_t sectors, uint8_t partNum) { // Set up the overlapped structure
  OVERLAPPED ovlWrite, ovlRead;
  int stride;
  uint64_t sec;
  DWORD bytesOut = 0;
  DWORD bytesRead = 0;
  int readStatus = ERROR_SUCCESS;
  int status = ERROR_SUCCESS;
  BOOL bWriteDone = TRUE;
  BOOL bBuffer1 = TRUE;

  if (sectorWrite < 0) {
    sectorWrite = GetNumDiskSectors() + sectorWrite;
  }

  // Set initial stride size to size of buffer
  stride = MAX_TRANSFER_SIZE / DISK_SECTOR_SIZE;

  // Setup offsets for read and write
  ovlWrite.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
  if (ovlWrite.hEvent == NULL)
    return ERROR_OUTOFMEMORY;
  ovlWrite.Offset = (DWORD) (sectorWrite * DISK_SECTOR_SIZE);
  ovlWrite.OffsetHigh = ((sectorWrite * DISK_SECTOR_SIZE) >> 32);

  ovlRead.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
  if (ovlRead.hEvent == NULL)
    return ERROR_OUTOFMEMORY;
  ovlRead.Offset = (DWORD) (sectorRead * DISK_SECTOR_SIZE);
  ovlRead.OffsetHigh = ((sectorRead * DISK_SECTOR_SIZE) >> 32);

  if (hRead == INVALID_HANDLE_VALUE) {
    // wprintf(L"hRead = INVALID_HANDLE_VALUE, zeroing input buffer\n");
    LDEBUG("DistWriter::FastCopy", "hRead的值为INVALID_HANDLE_VALUE，正在将输入缓冲区清零\n");
    memset(buffer1, 0, stride * DISK_SECTOR_SIZE);
    memset(buffer2, 0, stride * DISK_SECTOR_SIZE);
    bytesRead = stride * DISK_SECTOR_SIZE;
  }

  sec = 0;
  while (sec < sectors) {

    // Check if we have to read smaller number of sectors
    if ((sec + stride > sectors) && (sectors != 0)) {
      stride = (DWORD) (sectors - sec);
    }

    // If read handle is valid then read file file and wait for response
    if (hRead != INVALID_HANDLE_VALUE) {
      bytesRead = 0;
      if (bBuffer1)
        readStatus = ReadFile(hRead, buffer1, stride * DISK_SECTOR_SIZE, &bytesRead, &ovlRead);
      else
        readStatus = ReadFile(hRead, buffer2, stride * DISK_SECTOR_SIZE, &bytesRead, &ovlRead);
      if (!readStatus)
        status = GetLastError();
      if (status == ERROR_IO_PENDING)
        readStatus = GetOverlappedResult(hRead, &ovlRead, &bytesRead, TRUE);
      if (!readStatus) {
        status = GetLastError();
        break;
      }
      status = ERROR_SUCCESS;

      // Need to round up to nearest sector size if read partial sector from input file
      bytesRead = (bytesRead + DISK_SECTOR_SIZE - 1) & ~(DISK_SECTOR_SIZE - 1);
    }

    ovlRead.Offset += bytesRead;
    if (ovlRead.Offset < bytesRead)
      ovlRead.OffsetHigh++;

    if (!bWriteDone) {
      // Wait for the previous write to complete before we start a new one
      bWriteDone = GetOverlappedResult(hWrite, &ovlWrite, &bytesOut, TRUE);
      if (!bWriteDone) {
        status = GetLastError();
        break;
      }
      status = ERROR_SUCCESS;
    }

    // Now start a write for the corresponding buffer we just read
    if (bBuffer1)
      bWriteDone = WriteFile(hWrite, buffer1, bytesRead, NULL, &ovlWrite);
    else
      bWriteDone = WriteFile(hWrite, buffer2, bytesRead, NULL, &ovlWrite);

    bBuffer1 = !bBuffer1;
    sec += stride;
    sectorRead += stride;

    ovlWrite.Offset += bytesRead;
    if (ovlWrite.Offset < bytesOut)
      ovlWrite.OffsetHigh++;

    LDEBUG("DiskWriter::WriteDisk", "剩余扇区: %d      ", (int) (sectors - sec));
  }
  LINFO("DiskWriter::WriteDisk", "状态 = %d", status);

  // If we hit end of file and we read some data then round up to nearest block and write it out and wait
  // for it to complete else the next operation might fail.
  if ((sec < sectors) && (bytesRead > 0) && (status == ERROR_SUCCESS)) {
    bytesRead = (bytesRead + DISK_SECTOR_SIZE - 1) & ~(DISK_SECTOR_SIZE - 1);
    if (bBuffer1)
      bWriteDone = WriteFile(hWrite, buffer1, bytesRead, NULL, &ovlWrite);
    else
      bWriteDone = WriteFile(hWrite, buffer2, bytesRead, NULL, &ovlWrite);
    LINFO("DiskWriter::WriteDisk", "写入最后字节数: %d", (int) bytesRead);
  }

  // Wait for pending write transfers to complete
  if (!bWriteDone && (GetLastError() == ERROR_IO_PENDING)) {
    bWriteDone = GetOverlappedResult(hWrite, &ovlWrite, &bytesOut, TRUE);
    if (!bWriteDone)
      status = GetLastError();
  }

  CloseHandle(ovlRead.hEvent);
  CloseHandle(ovlWrite.hEvent);
  return status;
}

int DiskWriter::GetRawDiskSize(uint64_t* ds) {
  int status = ERROR_SUCCESS;
  // Start at 512 MB for good measure to get us to size quickly
  uint64_t diff = DISK_SECTOR_SIZE * 1024;

  // Read data from various sectors till we figure out how big disk is
  if (ds == NULL || hDisk == INVALID_HANDLE_VALUE) {
    return ERROR_INVALID_PARAMETER;
  }

  // Keep doubling size till we can't read anymore
  *ds = 0;
  for (;;) {
    status = RawReadTest(*ds + diff);
    if (diff <= DISK_SECTOR_SIZE) {
      if (status == ERROR_SUCCESS) {
        *ds = *ds + diff;
      }
      break;
    }
    if (status == ERROR_SUCCESS) {
      *ds = *ds + diff;
      diff = diff * 2;
    } else {
      diff = diff / 2;
    }
  }
  LDEBUG("DiskWriter::FindDiskSize", "磁盘大小值: %lld", *ds);

  return ERROR_SUCCESS;
}

int DiskWriter::LockDevice() {
  DWORD bytesRead = 0;
  if (DeviceIoControl(hDisk, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &bytesRead, NULL)) {
    if (DeviceIoControl(hDisk, FSCTL_LOCK_VOLUME, NULL, 0, NULL, 0, &bytesRead, NULL)) {
      LINFO("DiskWriter::LockDevice", "已锁定卷并卸载");
      return ERROR_SUCCESS;
    }
  }
  return ERROR_GEN_FAILURE;
}

int DiskWriter::UnlockDevice() {
  DWORD bytesRead = 0;
  if (DeviceIoControl(hDisk, FSCTL_UNLOCK_VOLUME, NULL, 0, NULL, 0, &bytesRead, NULL)) {
    return ERROR_SUCCESS;
  }
  return ERROR_GEN_FAILURE;
}