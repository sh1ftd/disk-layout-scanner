#ifndef DISK_INFO_H
#define DISK_INFO_H

#include "common.h"

#define MAX_DRIVES       64
#define MAX_VOLUMES       8
#define MAX_IDENTIFIERS  16
#define MAX_PARTITIONS   16
#define MAX_SMART_ATTRS  30

typedef struct {
    int  valid;
    char vendor[256];
    char product[256];
    char revision[64];
    char serial[256];
    int  bus_type;
    char bus_name[32];
    int  removable;
} StoragePropInfo;

typedef struct {
    int      valid;
    char     serial[48];
    char     firmware[16];
    char     model[64];
    UINT32   lba28;
    UINT64   lba48;
    double   capacity_gb;
    UINT64   wwn;
    UINT8    naa;
    UINT32   oui;
    UINT64   vendor_specific;
    UINT16   rpm;
    UINT16   cache_kb;
    int      is_ssd;
    UINT8    queue_depth;
    int      sata_gen;
    int      ncq_supported;
    int      trim_supported;
    int      lba48_supported;
    int      write_cache;
    int      smart_supported;
    int      smart_enabled;
    UINT16   log_sector_size;
    UINT16   phys_log_ratio;
    UINT8    form_factor;
    UINT16   ata_major;
    UINT8    udma_mode;
    UINT16   transport_major;
} AtaIdentifyInfo;

typedef struct {
    int  type;
    char type_name[32];
    int  code_set;
    char codeset_name[16];
    int  size;
    char data_hex[512];
    char data_ascii[256];
    int  is_ascii;
} VpdIdentifier;

typedef struct {
    int           valid;
    int           count;
    VpdIdentifier entries[MAX_IDENTIFIERS];
} DeviceIdsInfo;

typedef struct {
    UINT64 offset;
    UINT64 length;
    UINT32 number;
    BYTE   mbr_type;
    char   gpt_type_guid[64];
    char   gpt_name[128];
    int    is_gpt;
} PartitionInfo;

typedef struct {
    int    valid;
    int    style; /* 0=MBR, 1=GPT, 2=RAW */
    UINT32 mbr_signature;
    char   gpt_guid[64];
    UINT32 partition_count;
    int    detail_count;
    PartitionInfo parts[MAX_PARTITIONS];
} DriveLayoutInfo;

typedef struct {
    int    valid;
    UINT32 disk_sig;
    UINT16 boot_sig;
    BYTE   part1_type;
} RawMbrInfo;

typedef struct {
    int    valid;
    UINT64 total_bytes;
    UINT32 bytes_per_sector;
    UINT32 media_type;
    UINT64 cylinders;
    UINT32 heads;
    UINT32 sectors_per_track;
} GeometryInfo;

typedef struct {
    UINT8  id;
    char   name[32];
    UINT16 flags;
    UINT8  current;
    UINT8  worst;
    UINT64 raw;
} SmartAttr;

typedef struct {
    int       valid;
    /* 1 once SMART READ VALUES succeeded (attribute block available); not a full predictive-failure verdict */
    int       health_ok;
    int       attr_count;
    SmartAttr attrs[MAX_SMART_ATTRS];
    int       has_temp;
    UINT8     temperature;
    int       has_poh;
    UINT32    power_on_hours;
    int       has_power_cycles;
    UINT32    power_cycles;
    int       has_reallocated;
    UINT32    reallocated_sectors;
} SmartInfo;

typedef struct {
    int      valid;
    UINT8    critical_warning;
    UINT16   temperature; /* kelvin */
    UINT8    avail_spare;
    UINT8    avail_spare_thresh;
    UINT8    percent_used;
    UINT64   data_units_read;
    UINT64   data_units_written;
    UINT64   host_read_cmds;
    UINT64   host_write_cmds;
    UINT64   power_on_hours;
    UINT64   unsafe_shutdowns;
    UINT64   media_errors;
    UINT64   error_log_entries;
} NvmeHealthInfo;

typedef struct {
    int  valid;
    int  read_cache;
    int  write_cache;
    int  write_through;
    int  power_protected;
} CacheInfo;

typedef struct {
    int  valid;
    int  has_seek_penalty;
    int  trim_enabled;
    UINT32 align_byte_offset;
    int  align_valid;
} ExtraPropsInfo;

typedef struct {
    int    valid;
    UINT64 bytes_read;
    UINT64 bytes_written;
    UINT64 read_count;
    UINT64 write_count;
    UINT64 read_time_ns;
    UINT64 write_time_ns;
    UINT64 idle_time_ns;
    UINT32 queue_depth;
} PerfInfo;

typedef struct {
    int  valid;
    char device_path[512];
    char friendly_name[256];
    char hw_id[256];
    char location[256];
    char driver[128];
} DevPathInfo;

typedef struct {
    char   letter;
    UINT32 serial;
    char   fs_name[32];
    char   label[256];
    char   guid_path[128];
    char   mount_point[256];
} VolumeInfo;

typedef struct {
    int        count;
    VolumeInfo vols[MAX_VOLUMES];
} VolumesInfo;

typedef struct {
    int      valid;
    char     serial[24];
    char     model[44];
    char     firmware[12];
    UINT16   vid;
    UINT16   ssvid;
    UINT64   total_cap_bytes;
    UINT64   unalloc_cap_bytes;
    UINT8    num_namespaces;
    UINT16   max_transfer_sz;
    UINT16   ctrl_id;
    UINT32   ver;
    UINT8    ieee_oui[3];
} NvmeIdentifyInfo;

#define MAX_SMART_THRESH 30
typedef struct {
    UINT8 id;
    UINT8 threshold;
    int   exceeded;
} SmartThreshold;

typedef struct {
    int            valid;
    int            count;
    SmartThreshold entries[MAX_SMART_THRESH];
} SmartThreshInfo;

typedef struct {
    int  valid;
    int  supported;
    int  enabled;
    int  locked;
    int  frozen;
    int  count_expired;
    int  enhanced_erase;
    int  master_pwd_cap;
} AtaSecurityInfo;

typedef struct {
    int   valid;
    UINT8 mode;
    char  mode_name[32];
} PowerModeInfo;

#define MAX_FW_SLOTS 7
typedef struct {
    int   valid;
    UINT8 active_slot;
    UINT8 pending_slot;
    char  slot_rev[MAX_FW_SLOTS][12];
} NvmeFwSlotInfo;

typedef struct {
    int    valid;
    UINT64 native_max_lba;
    UINT64 current_max_lba;
    int    hpa_active;
    UINT64 hidden_sectors;
} HpaInfo;

typedef struct {
    int    valid;
    UINT64 real_max_lba;
    int    dco_active;
    int    features_disabled;
} DcoInfo;

typedef struct {
    int  valid;
    int  sed_capable;
    int  opal_v1;
    int  opal_v2;
    int  enterprise_ssc;
    int  ruby_ssc;
    int  pyrite_v1;
    int  pyrite_v2;
    int  locked;
    char desc[128];
} SedOpalInfo;

typedef struct {
    int   valid;
    int   write_cache;
    int   read_cache;
    int   awre;
    int   arre;
    UINT8 error_recovery;
} ScsiModePagesInfo;

typedef struct {
    int              drive_number;
    char             dev_path[64];
    StoragePropInfo  storage;
    AtaIdentifyInfo  ata;
    DeviceIdsInfo    ids;
    DriveLayoutInfo  layout;
    RawMbrInfo       mbr;
    GeometryInfo     geometry;
    SmartInfo        smart;
    SmartThreshInfo  smart_thresh;
    NvmeHealthInfo   nvme_health;
    NvmeIdentifyInfo nvme_id;
    NvmeFwSlotInfo   nvme_fw;
    CacheInfo        cache;
    ExtraPropsInfo   extra;
    PerfInfo         perf;
    DevPathInfo      devpath;
    VolumesInfo      volumes;
    AtaSecurityInfo  ata_sec;
    PowerModeInfo    power;
    HpaInfo          hpa;
    DcoInfo          dco;
    SedOpalInfo      sed;
    ScsiModePagesInfo scsi_modes;
} DiskInfo;

int  scan_disks(DiskInfo* disks, int max_disks);
void output_text(DiskInfo* disks, int count);
void output_json(DiskInfo* disks, int count, FILE* fp);
int output_html(DiskInfo* disks, int count, const char* filename);

#endif
