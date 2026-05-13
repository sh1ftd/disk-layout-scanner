#ifndef _WIN32
#define _GNU_SOURCE
#endif
#ifndef COMMON_H
#define COMMON_H

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <windows.h>
  #include <winioctl.h>
  #include <ntddscsi.h>
  #include <setupapi.h>
  #include <devguid.h>
  #include <cfgmgr32.h>
  #include <shellapi.h>

  #ifndef ATA_FLAGS_DATA_IN
  #define ATA_FLAGS_DATA_IN        0x02
  #endif
  #ifndef ATA_FLAGS_DRDY_REQUIRED
  #define ATA_FLAGS_DRDY_REQUIRED  0x01
  #endif
  #ifndef BusTypeNvme
  #define BusTypeNvme 17
  #endif
  #ifndef SMART_RCV_DRIVE_DATA
  #define SMART_RCV_DRIVE_DATA     0x0007C088
  #endif
  #ifndef SMART_SEND_DRIVE_COMMAND
  #define SMART_SEND_DRIVE_COMMAND 0x0007C084
  #endif
  #ifndef StorageDeviceProtocolSpecificProperty
  #define StorageDeviceProtocolSpecificProperty 49
  #endif
  #ifndef ProtocolTypeNvme
  #define ProtocolTypeNvme 3
  #endif
  #ifndef NVMeDataTypeLogPage
  #define NVMeDataTypeLogPage 2
  #endif
  #ifndef NVME_LOG_PAGE_HEALTH_INFO
  #define NVME_LOG_PAGE_HEALTH_INFO 0x02
  #endif
#else
  #include <unistd.h>
  #include <fcntl.h>
  #include <sys/ioctl.h>
  #include <linux/hdreg.h>
  #include <scsi/sg.h>
  #include <linux/nvme_ioctl.h>
  #include <dirent.h>
  #include <sys/stat.h>
  #include <errno.h>
  #include <ctype.h>
  #include <glob.h>

  typedef unsigned char      BYTE;
  typedef unsigned char      UINT8;
  typedef unsigned short     UINT16;
  typedef unsigned short     WORD;
  typedef unsigned int       UINT32;
  typedef unsigned int       DWORD;
  typedef unsigned int       BOOL;
  typedef unsigned long long UINT64;
  typedef void*              HANDLE;
  #define INVALID_HANDLE_VALUE ((HANDLE)(long long)-1)
  #define TRUE  1
  #define FALSE 0
  #define MAX_PATH 260
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>

#ifndef DISK_LAYOUT_SCANNER_VERSION
#define DISK_LAYOUT_SCANNER_VERSION "0.0.0-dev"
#endif

#define ATA_CMD_IDENTIFY_DEVICE  0xEC
#define SMART_CMD                0xB0
#define SMART_READ_VALUES        0xD0
#define SMART_RETURN_STATUS      0xDA
#define SMART_ENABLE_OPS         0xD8

void print_section(const char* title);
const char* bus_type_name(int bus_type);
const char* id_type_name(int type);
const char* codeset_name(int cs);
void ata_words_to_string(const WORD* words, int start, int end, char* out, int out_sz);
const char* smart_attr_name(UINT8 id);
const char* form_factor_name(UINT8 ff);
const char* ata_version_name(UINT16 major);

#endif
