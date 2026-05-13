#ifdef _WIN32
#include "../core/disk_info.h"
#include <stddef.h>

#pragma pack(push, 1)
typedef struct {
    BYTE  bFeaturesReg;
    BYTE  bSectorCountReg;
    BYTE  bSectorNumberReg;
    BYTE  bCylLowReg;
    BYTE  bCylHighReg;
    BYTE  bDriveHeadReg;
    BYTE  bCommandReg;
    BYTE  bReserved;
} IDEREGS_S;

typedef struct {
    DWORD    cBufferSize;
    IDEREGS_S irDriveRegs;
    BYTE     bDriveNumber;
    BYTE     bReserved[3];
    DWORD    dwReserved[4];
    BYTE     bBuffer[1];
} SENDCMDINPARAMS_S;

typedef struct {
    BYTE  bDriverError;
    BYTE  bIDEStatus;
    BYTE  bReserved[2];
    DWORD dwReserved[2];
} DRIVERSTATUS_S;

typedef struct {
    DWORD          cBufferSize;
    DRIVERSTATUS_S DriverStatus;
    BYTE           bBuffer[512];
} SENDCMDOUTPARAMS_S;
#pragma pack(pop)

#include <initguid.h>
#include <ntddstor.h>

void query_storage_prop(HANDLE hDisk, StoragePropInfo* out) {
    memset(out, 0, sizeof(*out));
    STORAGE_PROPERTY_QUERY spq;
    memset(&spq, 0, sizeof(spq));
    spq.PropertyId = StorageDeviceProperty;
    spq.QueryType  = PropertyStandardQuery;

    BYTE buf[4096];
    DWORD ret = 0;
    if (!DeviceIoControl(hDisk, IOCTL_STORAGE_QUERY_PROPERTY,
                         &spq, sizeof(spq), buf, sizeof(buf), &ret, NULL)) return;
    out->valid = 1;
    STORAGE_DEVICE_DESCRIPTOR* d = (STORAGE_DEVICE_DESCRIPTOR*)buf;
    if (d->VendorIdOffset && d->VendorIdOffset < ret)
        strncpy(out->vendor, (char*)buf + d->VendorIdOffset, sizeof(out->vendor)-1);
    if (d->ProductIdOffset && d->ProductIdOffset < ret)
        strncpy(out->product, (char*)buf + d->ProductIdOffset, sizeof(out->product)-1);
    if (d->ProductRevisionOffset && d->ProductRevisionOffset < ret)
        strncpy(out->revision, (char*)buf + d->ProductRevisionOffset, sizeof(out->revision)-1);
    if (d->SerialNumberOffset && d->SerialNumberOffset < ret)
        strncpy(out->serial, (char*)buf + d->SerialNumberOffset, sizeof(out->serial)-1);
    out->bus_type = d->BusType;
    strncpy(out->bus_name, bus_type_name(d->BusType), sizeof(out->bus_name)-1);
    out->removable = d->RemovableMedia;
}

void query_ata_id(HANDLE hDisk, AtaIdentifyInfo* out) {
    memset(out, 0, sizeof(*out));
    BYTE identify[512];
    memset(identify, 0, sizeof(identify));

    ATA_PASS_THROUGH_DIRECT aptd;
    memset(&aptd, 0, sizeof(aptd));
    aptd.Length             = sizeof(ATA_PASS_THROUGH_DIRECT);
    aptd.AtaFlags           = ATA_FLAGS_DATA_IN | ATA_FLAGS_DRDY_REQUIRED;
    aptd.DataTransferLength = 512;
    aptd.TimeOutValue       = 5;
    aptd.DataBuffer         = identify;
    aptd.CurrentTaskFile[6] = ATA_CMD_IDENTIFY_DEVICE;

    DWORD ret = 0;
    if (!DeviceIoControl(hDisk, IOCTL_ATA_PASS_THROUGH_DIRECT,
                         &aptd, sizeof(aptd), &aptd, sizeof(aptd), &ret, NULL)) return;
    out->valid = 1;
    WORD* w = (WORD*)identify;

    ata_words_to_string(w, 10, 19, out->serial, sizeof(out->serial));
    ata_words_to_string(w, 23, 26, out->firmware, sizeof(out->firmware));
    ata_words_to_string(w, 27, 46, out->model, sizeof(out->model));

    out->lba28 = (UINT32)w[60] | ((UINT32)w[61] << 16);
    out->lba48 = (UINT64)w[100] | ((UINT64)w[101] << 16)
               | ((UINT64)w[102] << 32) | ((UINT64)w[103] << 48);
    UINT64 secs = out->lba48 > 0 ? out->lba48 : out->lba28;
    out->capacity_gb = (double)secs * 512.0 / 1e9;

    for (int i = 108; i <= 111; i++)
        out->wwn = (out->wwn << 16) | w[i];
    if (out->wwn) {
        out->naa = (UINT8)(out->wwn >> 60);
        if (out->naa == 5 || out->naa == 6)
            out->oui = (UINT32)((out->wwn >> 36) & 0xFFFFFF);
        if (out->naa == 5)
            out->vendor_specific = out->wwn & 0xFFFFFFFFFULL;
    }

    if (w[217] == 0x0001) out->is_ssd = 1;
    else if (w[217] >= 0x0401 && w[217] <= 0xFFFE) out->rpm = w[217];
    if (w[21]) out->cache_kb = w[21] * 512 / 1024;


    out->queue_depth = (w[75] & 0x1F) + 1;

    if (w[76] != 0x0000 && w[76] != 0xFFFF) {
        if (w[76] & (1 << 3)) out->sata_gen = 3;
        else if (w[76] & (1 << 2)) out->sata_gen = 2;
        else if (w[76] & (1 << 1)) out->sata_gen = 1;
        out->ncq_supported = (w[76] >> 8) & 1;
    }

    out->smart_supported  = (w[82] >> 0) & 1;
    out->lba48_supported  = (w[83] >> 10) & 1;
    out->smart_enabled    = (w[85] >> 0) & 1;
    out->write_cache      = (w[85] >> 5) & 1;

    out->trim_supported = (w[169] >> 0) & 1;

    if (w[106] & (1 << 14)) {
        out->phys_log_ratio = 1 << (w[106] & 0x0F);
        if (w[106] & (1 << 12)) {
            out->log_sector_size = (UINT16)(((UINT32)w[117] | ((UINT32)w[118] << 16)) * 2);
        } else {
            out->log_sector_size = 512;
        }
    } else {
        out->log_sector_size = 512;
        out->phys_log_ratio = 1;
    }

    out->form_factor = (UINT8)(w[168] & 0x0F);

    out->ata_major = w[80];

    if (w[88] & 0x7F00) {
        UINT8 active = (w[88] >> 8) & 0x7F;
        out->udma_mode = 0;
        for (int b = 6; b >= 0; b--) {
            if (active & (1 << b)) { out->udma_mode = (UINT8)b; break; }
        }
    }

    out->transport_major = w[222];
}

void query_dev_ids(HANDLE hDisk, DeviceIdsInfo* out) {
    memset(out, 0, sizeof(*out));
    STORAGE_PROPERTY_QUERY spq;
    memset(&spq, 0, sizeof(spq));
    spq.PropertyId = StorageDeviceIdProperty;
    spq.QueryType  = PropertyStandardQuery;

    BYTE buf[4096];
    DWORD ret = 0;
    if (!DeviceIoControl(hDisk, IOCTL_STORAGE_QUERY_PROPERTY,
                         &spq, sizeof(spq), buf, sizeof(buf), &ret, NULL)) return;
    if (ret < sizeof(STORAGE_DEVICE_ID_DESCRIPTOR)) return;

    STORAGE_DEVICE_ID_DESCRIPTOR* idDesc = (STORAGE_DEVICE_ID_DESCRIPTOR*)buf;
    DWORD descLen = idDesc->Size;
    if (descLen > ret) descLen = ret;
    if (descLen > sizeof(buf)) descLen = (DWORD)sizeof(buf);
    if (descLen < sizeof(STORAGE_DEVICE_ID_DESCRIPTOR)) return;

    const size_t id_ident_off = offsetof(STORAGE_IDENTIFIER, Identifier);
    BYTE* ptr = idDesc->Identifiers;
    BYTE* end = buf + descLen;
    DWORD max_ids = idDesc->NumberOfIdentifiers;
    if (max_ids > MAX_IDENTIFIERS) max_ids = MAX_IDENTIFIERS;

    for (DWORD i = 0; i < max_ids && out->count < MAX_IDENTIFIERS; i++) {
        if (ptr + id_ident_off > end) { memset(out, 0, sizeof(*out)); return; }
        STORAGE_IDENTIFIER* sid = (STORAGE_IDENTIFIER*)ptr;
        DWORD idsz = sid->IdentifierSize;
        if (idsz > (DWORD)(end - ptr - id_ident_off)) { memset(out, 0, sizeof(*out)); return; }

        VpdIdentifier* e = &out->entries[out->count];
        e->type = sid->Type;
        strncpy(e->type_name, id_type_name(sid->Type), sizeof(e->type_name)-1);
        e->code_set = sid->CodeSet;
        strncpy(e->codeset_name, codeset_name(sid->CodeSet), sizeof(e->codeset_name)-1);
        e->size = (int)idsz;

        if (sid->CodeSet == StorageIdCodeSetAscii || sid->CodeSet == StorageIdCodeSetUtf8) {
            e->is_ascii = 1;
            int len = (int)idsz < (int)sizeof(e->data_ascii)-1 ? (int)idsz : (int)sizeof(e->data_ascii)-1;
            memcpy(e->data_ascii, sid->Identifier, (size_t)len);
            e->data_ascii[len] = '\0';
        }
        for (int b = 0; b < (int)idsz && b*2 < (int)sizeof(e->data_hex)-2; b++)
            sprintf(e->data_hex + b*2, "%02X", sid->Identifier[b]);

        out->count++;
        DWORD next = sid->NextOffset;
        if (next == 0) break;
        if (next < (DWORD)id_ident_off + idsz || ptr + next > end) { memset(out, 0, sizeof(*out)); return; }
        ptr += next;
    }
    out->valid = 1;
}

void query_layout(HANDLE hDisk, DriveLayoutInfo* out) {
    memset(out, 0, sizeof(*out));
    BYTE buf[32768];
    DWORD ret = 0;
    if (!DeviceIoControl(hDisk, IOCTL_DISK_GET_DRIVE_LAYOUT_EX,
                         NULL, 0, buf, sizeof(buf), &ret, NULL)) return;
    out->valid = 1;
    DRIVE_LAYOUT_INFORMATION_EX* lay = (DRIVE_LAYOUT_INFORMATION_EX*)buf;
    out->partition_count = lay->PartitionCount;

    if (lay->PartitionStyle == PARTITION_STYLE_MBR) {
        out->style = 0;
        out->mbr_signature = lay->Mbr.Signature;
    } else if (lay->PartitionStyle == PARTITION_STYLE_GPT) {
        out->style = 1;
        GUID g = lay->Gpt.DiskId;
        sprintf(out->gpt_guid, "{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
                g.Data1, g.Data2, g.Data3,
                g.Data4[0], g.Data4[1], g.Data4[2], g.Data4[3],
                g.Data4[4], g.Data4[5], g.Data4[6], g.Data4[7]);
    } else {
        out->style = 2;
    }

    for (DWORD p = 0; p < lay->PartitionCount && out->detail_count < MAX_PARTITIONS; p++) {
        PARTITION_INFORMATION_EX* pe = &lay->PartitionEntry[p];
        if (pe->PartitionLength.QuadPart == 0) continue;
        PartitionInfo* pi = &out->parts[out->detail_count];
        pi->offset = pe->StartingOffset.QuadPart;
        pi->length = pe->PartitionLength.QuadPart;
        pi->number = pe->PartitionNumber;
        if (pe->PartitionStyle == PARTITION_STYLE_MBR) {
            pi->is_gpt = 0;
            pi->mbr_type = pe->Mbr.PartitionType;
        } else if (pe->PartitionStyle == PARTITION_STYLE_GPT) {
            pi->is_gpt = 1;
            GUID g = pe->Gpt.PartitionType;
            sprintf(pi->gpt_type_guid, "{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
                    g.Data1, g.Data2, g.Data3,
                    g.Data4[0], g.Data4[1], g.Data4[2], g.Data4[3],
                    g.Data4[4], g.Data4[5], g.Data4[6], g.Data4[7]);
            WideCharToMultiByte(CP_UTF8, 0, pe->Gpt.Name, -1, pi->gpt_name, sizeof(pi->gpt_name), NULL, NULL);
        }
        out->detail_count++;
    }
}

void query_raw_mbr(HANDLE hDisk, RawMbrInfo* out) {
    memset(out, 0, sizeof(*out));
    BYTE mbr[512];
    DWORD bytesRead = 0;
    LARGE_INTEGER pos;
    pos.QuadPart = 0;
    SetFilePointerEx(hDisk, pos, NULL, FILE_BEGIN);
    if (!ReadFile(hDisk, mbr, 512, &bytesRead, NULL) || bytesRead < 512) return;
    out->valid = 1;
    out->disk_sig   = *(UINT32*)(mbr + 0x1B8);
    out->boot_sig   = *(UINT16*)(mbr + 0x1FE);
    out->part1_type = mbr[0x1C2];
}

void query_geometry(HANDLE hDisk, GeometryInfo* out) {
    memset(out, 0, sizeof(*out));
    DISK_GEOMETRY_EX geo;
    memset(&geo, 0, sizeof(geo));
    DWORD ret = 0;
    if (!DeviceIoControl(hDisk, IOCTL_DISK_GET_DRIVE_GEOMETRY_EX,
                         NULL, 0, &geo, sizeof(geo), &ret, NULL)) return;
    out->valid = 1;
    out->total_bytes      = geo.DiskSize.QuadPart;
    out->bytes_per_sector = geo.Geometry.BytesPerSector;
    out->media_type       = geo.Geometry.MediaType;
    out->cylinders        = geo.Geometry.Cylinders.QuadPart;
    out->heads            = geo.Geometry.TracksPerCylinder;
    out->sectors_per_track = geo.Geometry.SectorsPerTrack;
}

void query_smart(HANDLE hDisk, SmartInfo* out) {
    memset(out, 0, sizeof(*out));

    BYTE cmd_buf[sizeof(SENDCMDINPARAMS_S) + 16];
    memset(cmd_buf, 0, sizeof(cmd_buf));
    SENDCMDINPARAMS_S* cmd = (SENDCMDINPARAMS_S*)cmd_buf;
    cmd->cBufferSize = 0;
    cmd->irDriveRegs.bFeaturesReg    = SMART_ENABLE_OPS;
    cmd->irDriveRegs.bCylLowReg      = 0x4F;
    cmd->irDriveRegs.bCylHighReg     = 0xC2;
    cmd->irDriveRegs.bCommandReg     = SMART_CMD;
    cmd->irDriveRegs.bSectorCountReg = 1;
    cmd->irDriveRegs.bSectorNumberReg = 1;

    SENDCMDOUTPARAMS_S enable_out;
    DWORD ret = 0;
    DeviceIoControl(hDisk, SMART_SEND_DRIVE_COMMAND,
                    cmd_buf, sizeof(cmd_buf), &enable_out, sizeof(enable_out), &ret, NULL);

    memset(cmd_buf, 0, sizeof(cmd_buf));
    cmd = (SENDCMDINPARAMS_S*)cmd_buf;
    cmd->cBufferSize = 0;
    cmd->irDriveRegs.bFeaturesReg     = SMART_RETURN_STATUS;
    cmd->irDriveRegs.bCylLowReg       = 0x4F;
    cmd->irDriveRegs.bCylHighReg      = 0xC2;
    cmd->irDriveRegs.bCommandReg      = SMART_CMD;
    cmd->irDriveRegs.bSectorCountReg  = 1;
    cmd->irDriveRegs.bSectorNumberReg = 1;

    BYTE status_buf[sizeof(SENDCMDOUTPARAMS_S) + 16];
    memset(status_buf, 0, sizeof(status_buf));
    DeviceIoControl(hDisk, SMART_SEND_DRIVE_COMMAND,
                    cmd_buf, sizeof(cmd_buf), status_buf, sizeof(status_buf), &ret, NULL);

    memset(cmd_buf, 0, sizeof(cmd_buf));
    cmd = (SENDCMDINPARAMS_S*)cmd_buf;
    cmd->cBufferSize = 512;
    cmd->irDriveRegs.bFeaturesReg     = SMART_READ_VALUES;
    cmd->irDriveRegs.bCylLowReg       = 0x4F;
    cmd->irDriveRegs.bCylHighReg      = 0xC2;
    cmd->irDriveRegs.bCommandReg      = SMART_CMD;
    cmd->irDriveRegs.bSectorCountReg  = 1;
    cmd->irDriveRegs.bSectorNumberReg = 1;

    SENDCMDOUTPARAMS_S attr_out;
    memset(&attr_out, 0, sizeof(attr_out));

    if (!DeviceIoControl(hDisk, SMART_RCV_DRIVE_DATA,
                         cmd_buf, sizeof(cmd_buf), &attr_out, sizeof(attr_out), &ret, NULL)) {
        return;
    }

    out->valid = 1;
    out->health_ok = 1;
    BYTE* data = attr_out.bBuffer;

    for (int i = 0; i < 30 && out->attr_count < MAX_SMART_ATTRS; i++) {
        BYTE* entry = data + 2 + (i * 12);
        UINT8 id = entry[0];
        if (id == 0) continue;

        SmartAttr* a = &out->attrs[out->attr_count];
        a->id      = id;
        a->flags   = *(UINT16*)(entry + 1);
        a->current = entry[3];
        a->worst   = entry[4];
        a->raw     = 0;
        for (int b = 5; b >= 0; b--)
            a->raw = (a->raw << 8) | entry[5 + b];

        const char* name = smart_attr_name(id);
        if (name[0]) strncpy(a->name, name, sizeof(a->name)-1);
        else sprintf(a->name, "Attr 0x%02X", id);

        if (id == 0xC2 || id == 0xBE) {
            out->has_temp = 1;
            out->temperature = (UINT8)(a->raw & 0xFF);
        }
        if (id == 0x09) {
            out->has_poh = 1;
            out->power_on_hours = (UINT32)(a->raw & 0xFFFFFFFF);
        }
        if (id == 0x0C) {
            out->has_power_cycles = 1;
            out->power_cycles = (UINT32)(a->raw & 0xFFFFFFFF);
        }
        if (id == 0x05) {
            out->has_reallocated = 1;
            out->reallocated_sectors = (UINT32)(a->raw & 0xFFFFFFFF);
        }

        out->attr_count++;
    }
}

void query_vols(int disk_number, VolumesInfo* out) {
    memset(out, 0, sizeof(*out));
    for (char letter = 'A'; letter <= 'Z' && out->count < MAX_VOLUMES; letter++) {
        char vol_path[8], vol_dev[16];
        sprintf(vol_path, "%c:\\", letter);
        if (GetDriveTypeA(vol_path) == DRIVE_NO_ROOT_DIR) continue;

        sprintf(vol_dev, "\\\\.\\%c:", letter);
        HANDLE hVol = CreateFileA(vol_dev, 0, FILE_SHARE_READ|FILE_SHARE_WRITE,
                                   NULL, OPEN_EXISTING, 0, NULL);
        if (hVol == INVALID_HANDLE_VALUE) continue;

        BYTE ext_buf[256];
        memset(ext_buf, 0, sizeof(ext_buf));
        DWORD ret = 0;
        BOOL ok = DeviceIoControl(hVol, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS,
                                   NULL, 0, ext_buf, sizeof(ext_buf), &ret, NULL);
        CloseHandle(hVol);
        if (!ok) continue;

        VOLUME_DISK_EXTENTS* ext = (VOLUME_DISK_EXTENTS*)ext_buf;
        BOOL match = FALSE;
        for (DWORD e = 0; e < ext->NumberOfDiskExtents; e++)
            if ((int)ext->Extents[e].DiskNumber == disk_number) { match = TRUE; break; }
        if (!match) continue;

        VolumeInfo* v = &out->vols[out->count];
        v->letter = letter;
        DWORD serial = 0;
        if (GetVolumeInformationA(vol_path, v->label, sizeof(v->label),
                                   &serial, NULL, NULL, v->fs_name, sizeof(v->fs_name)))
        {
            v->serial = serial;

            char mount[16];
            sprintf(mount, "%c:\\", letter);
            GetVolumeNameForVolumeMountPointA(mount, v->guid_path, sizeof(v->guid_path));

            out->count++;
        }
    }
}

#pragma pack(push, 1)
typedef struct {
    STORAGE_PROPERTY_QUERY Query;
    DWORD ProtocolType;
    DWORD DataType;
    DWORD ProtocolDataRequestValue;
    DWORD ProtocolDataRequestSubValue;
    DWORD ProtocolDataOffset;
    DWORD ProtocolDataLength;
    DWORD FixedProtocolReturnData;
    DWORD ProtocolDataRequestSubValue2;
    DWORD ProtocolDataRequestSubValue3;
    DWORD Reserved;
} PROTO_QUERY;
#pragma pack(pop)

void query_nvme_health(HANDLE hDisk, NvmeHealthInfo* out) {
    memset(out, 0, sizeof(*out));

    BYTE buf[4096];
    memset(buf, 0, sizeof(buf));

    PROTO_QUERY* pq = (PROTO_QUERY*)buf;
    pq->Query.PropertyId = (STORAGE_PROPERTY_ID)StorageDeviceProtocolSpecificProperty;
    pq->Query.QueryType  = PropertyStandardQuery;
    pq->ProtocolType     = ProtocolTypeNvme;
    pq->DataType         = NVMeDataTypeLogPage;
    pq->ProtocolDataRequestValue    = NVME_LOG_PAGE_HEALTH_INFO;
    pq->ProtocolDataRequestSubValue = 0;
    pq->ProtocolDataOffset = sizeof(PROTO_QUERY) - sizeof(STORAGE_PROPERTY_QUERY);
    pq->ProtocolDataLength = 512;

    BYTE out_buf[4096];
    memset(out_buf, 0, sizeof(out_buf));
    DWORD ret = 0;

    if (!DeviceIoControl(hDisk, IOCTL_STORAGE_QUERY_PROPERTY,
                         buf, sizeof(PROTO_QUERY), out_buf, sizeof(out_buf), &ret, NULL))
        return;

    BYTE* log = out_buf + sizeof(STORAGE_PROPERTY_QUERY)
              + (sizeof(PROTO_QUERY) - sizeof(STORAGE_PROPERTY_QUERY));
    if (ret < sizeof(PROTO_QUERY) + 32) return;

    out->valid = 1;
    out->critical_warning = log[0];
    out->temperature = (UINT16)(log[1] | (log[2] << 8));  /* Kelvin */
    out->avail_spare = log[3];
    out->avail_spare_thresh = log[4];
    out->percent_used = log[5];

    memcpy(&out->data_units_read, log + 32, 8);
    memcpy(&out->data_units_written, log + 48, 8);
    memcpy(&out->host_read_cmds, log + 64, 8);
    memcpy(&out->host_write_cmds, log + 80, 8);
    memcpy(&out->power_on_hours, log + 128, 8);
    memcpy(&out->unsafe_shutdowns, log + 144, 8);
    memcpy(&out->media_errors, log + 160, 8);
    memcpy(&out->error_log_entries, log + 176, 8);
}

void query_cache(HANDLE hDisk, CacheInfo* out) {
    memset(out, 0, sizeof(*out));
    DISK_CACHE_INFORMATION ci;
    memset(&ci, 0, sizeof(ci));
    DWORD ret = 0;

    if (!DeviceIoControl(hDisk, IOCTL_DISK_GET_CACHE_INFORMATION,
                         NULL, 0, &ci, sizeof(ci), &ret, NULL)) return;

    out->valid = 1;
    out->read_cache     = ci.ReadCacheEnabled;
    out->write_cache    = ci.WriteCacheEnabled;
    out->write_through  = ci.WriteCacheEnabled ? 0 : 1;
}

void query_extra_props(HANDLE hDisk, ExtraPropsInfo* out) {
    memset(out, 0, sizeof(*out));
    out->valid = 1;
    DWORD ret = 0;

    {
        BYTE buf[256];
        memset(buf, 0, sizeof(buf));
        STORAGE_PROPERTY_QUERY spq;
        memset(&spq, 0, sizeof(spq));
        spq.PropertyId = (STORAGE_PROPERTY_ID)7; /* StorageDeviceSeekPenaltyProperty */
        spq.QueryType  = PropertyStandardQuery;
        if (DeviceIoControl(hDisk, IOCTL_STORAGE_QUERY_PROPERTY,
                            &spq, sizeof(spq), buf, sizeof(buf), &ret, NULL)) {
            out->has_seek_penalty = *(BOOL*)(buf + 8) ? 1 : 0;
        }
    }

    {
        BYTE buf[256];
        memset(buf, 0, sizeof(buf));
        STORAGE_PROPERTY_QUERY spq;
        memset(&spq, 0, sizeof(spq));
        spq.PropertyId = (STORAGE_PROPERTY_ID)8; /* StorageDeviceTrimProperty */
        spq.QueryType  = PropertyStandardQuery;
        if (DeviceIoControl(hDisk, IOCTL_STORAGE_QUERY_PROPERTY,
                            &spq, sizeof(spq), buf, sizeof(buf), &ret, NULL)) {
            out->trim_enabled = *(BOOL*)(buf + 8) ? 1 : 0;
        }
    }

    {
        BYTE buf[256];
        memset(buf, 0, sizeof(buf));
        STORAGE_PROPERTY_QUERY spq;
        memset(&spq, 0, sizeof(spq));
        spq.PropertyId = (STORAGE_PROPERTY_ID)6; /* StorageAccessAlignmentProperty */
        spq.QueryType  = PropertyStandardQuery;
        if (DeviceIoControl(hDisk, IOCTL_STORAGE_QUERY_PROPERTY,
                            &spq, sizeof(spq), buf, sizeof(buf), &ret, NULL) && ret >= 16) {
            out->align_byte_offset = *(UINT32*)(buf + 12);
            out->align_valid = 1;
        }
    }
}

void query_perf(HANDLE hDisk, PerfInfo* out) {
    memset(out, 0, sizeof(*out));
    DISK_PERFORMANCE dp;
    memset(&dp, 0, sizeof(dp));
    DWORD ret = 0;

    if (!DeviceIoControl(hDisk, IOCTL_DISK_PERFORMANCE,
                         NULL, 0, &dp, sizeof(dp), &ret, NULL)) return;

    out->valid = 1;
    out->bytes_read    = dp.BytesRead.QuadPart;
    out->bytes_written = dp.BytesWritten.QuadPart;
    out->read_count    = dp.ReadCount;
    out->write_count   = dp.WriteCount;
    out->read_time_ns  = dp.ReadTime.QuadPart * 100;   /* 100ns ticks -> ns */
    out->write_time_ns = dp.WriteTime.QuadPart * 100;
    out->idle_time_ns  = dp.IdleTime.QuadPart * 100;
    out->queue_depth   = dp.QueueDepth;
}

void query_devpath(int disk_number, DevPathInfo* out) {
    memset(out, 0, sizeof(*out));

    HDEVINFO devInfoSet = SetupDiGetClassDevsA(
        &GUID_DEVINTERFACE_DISK, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (devInfoSet == INVALID_HANDLE_VALUE) return;

    SP_DEVICE_INTERFACE_DATA ifData;
    ifData.cbSize = sizeof(ifData);

    for (DWORD idx = 0; SetupDiEnumDeviceInterfaces(devInfoSet, NULL, &GUID_DEVINTERFACE_DISK, idx, &ifData); idx++) {
        DWORD need = 0;
        SetupDiGetDeviceInterfaceDetailA(devInfoSet, &ifData, NULL, 0, &need, NULL);
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || need < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A))
            continue;

        SP_DEVICE_INTERFACE_DETAIL_DATA_A* detail = (SP_DEVICE_INTERFACE_DETAIL_DATA_A*)malloc(need);
        if (!detail) continue;
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);
        SP_DEVINFO_DATA devInfo;
        devInfo.cbSize = sizeof(SP_DEVINFO_DATA);
        if (!SetupDiGetDeviceInterfaceDetailA(devInfoSet, &ifData, detail, need, &need, &devInfo)) {
            free(detail);
            continue;
        }

        HANDLE h = CreateFileA(detail->DevicePath, 0,
                               FILE_SHARE_READ | FILE_SHARE_WRITE,
                               NULL, OPEN_EXISTING, 0, NULL);
        if (h == INVALID_HANDLE_VALUE) {
            free(detail);
            continue;
        }

        STORAGE_DEVICE_NUMBER sdn;
        DWORD br = 0;
        int match = DeviceIoControl(h, IOCTL_STORAGE_GET_DEVICE_NUMBER, NULL, 0,
                                    &sdn, sizeof(sdn), &br, NULL)
                 && br >= sizeof(sdn)
                 && (int)sdn.DeviceNumber == disk_number;
        CloseHandle(h);

        if (!match) {
            free(detail);
            continue;
        }

        char friendly[256] = {0};
        SetupDiGetDeviceRegistryPropertyA(devInfoSet, &devInfo, SPDRP_FRIENDLYNAME,
                                           NULL, (BYTE*)friendly, sizeof(friendly), NULL);

        char hwid[256] = {0};
        SetupDiGetDeviceRegistryPropertyA(devInfoSet, &devInfo, SPDRP_HARDWAREID,
                                           NULL, (BYTE*)hwid, sizeof(hwid), NULL);

        char location[256] = {0};
        SetupDiGetDeviceRegistryPropertyA(devInfoSet, &devInfo, SPDRP_LOCATION_INFORMATION,
                                           NULL, (BYTE*)location, sizeof(location), NULL);

        char driver[128] = {0};
        SetupDiGetDeviceRegistryPropertyA(devInfoSet, &devInfo, SPDRP_DRIVER,
                                           NULL, (BYTE*)driver, sizeof(driver), NULL);

        out->valid = 1;
        strncpy(out->device_path, detail->DevicePath, sizeof(out->device_path)-1);
        strncpy(out->friendly_name, friendly, sizeof(out->friendly_name)-1);
        strncpy(out->hw_id, hwid, sizeof(out->hw_id)-1);
        strncpy(out->location, location, sizeof(out->location)-1);
        strncpy(out->driver, driver, sizeof(out->driver)-1);
        free(detail);
        break;
    }

    SetupDiDestroyDeviceInfoList(devInfoSet);
}

static void query_smart_thresh(HANDLE hDisk, SmartThreshInfo* out, SmartInfo* smart) {
    memset(out, 0, sizeof(*out));
    BYTE cmd_buf[sizeof(SENDCMDINPARAMS_S) + 16];
    memset(cmd_buf, 0, sizeof(cmd_buf));
    SENDCMDINPARAMS_S* cmd = (SENDCMDINPARAMS_S*)cmd_buf;
    cmd->cBufferSize = 512;
    cmd->irDriveRegs.bFeaturesReg     = 0xD1; /* SMART_READ_THRESHOLDS */
    cmd->irDriveRegs.bCylLowReg       = 0x4F;
    cmd->irDriveRegs.bCylHighReg      = 0xC2;
    cmd->irDriveRegs.bCommandReg      = SMART_CMD;
    cmd->irDriveRegs.bSectorCountReg  = 1;
    cmd->irDriveRegs.bSectorNumberReg = 1;

    SENDCMDOUTPARAMS_S thresh_out;
    memset(&thresh_out, 0, sizeof(thresh_out));
    DWORD ret = 0;
    if (!DeviceIoControl(hDisk, SMART_RCV_DRIVE_DATA,
                         cmd_buf, sizeof(cmd_buf), &thresh_out, sizeof(thresh_out), &ret, NULL))
        return;

    out->valid = 1;
    BYTE* data = thresh_out.bBuffer;
    for (int i = 0; i < 30 && out->count < MAX_SMART_THRESH; i++) {
        BYTE* entry = data + 2 + (i * 12);
        UINT8 id = entry[0];
        UINT8 thresh = entry[1];
        if (id == 0) continue;
        SmartThreshold* t = &out->entries[out->count];
        t->id = id;
        t->threshold = thresh;
        for (int j = 0; j < smart->attr_count; j++) {
            if (smart->attrs[j].id == id && smart->attrs[j].current < thresh) {
                t->exceeded = 1; break;
            }
        }
        out->count++;
    }
}

static void query_ata_security(HANDLE hDisk, AtaSecurityInfo* out) {
    memset(out, 0, sizeof(*out));
    BYTE identify[512];
    memset(identify, 0, sizeof(identify));
    ATA_PASS_THROUGH_DIRECT aptd;
    memset(&aptd, 0, sizeof(aptd));
    aptd.Length             = sizeof(ATA_PASS_THROUGH_DIRECT);
    aptd.AtaFlags           = ATA_FLAGS_DATA_IN | ATA_FLAGS_DRDY_REQUIRED;
    aptd.DataTransferLength = 512;
    aptd.TimeOutValue       = 5;
    aptd.DataBuffer         = identify;
    aptd.CurrentTaskFile[6] = ATA_CMD_IDENTIFY_DEVICE;
    DWORD ret = 0;
    if (!DeviceIoControl(hDisk, IOCTL_ATA_PASS_THROUGH_DIRECT,
                         &aptd, sizeof(aptd), &aptd, sizeof(aptd), &ret, NULL)) return;

    WORD* w = (WORD*)identify;
    UINT16 sec_word = w[128];
    out->valid = 1;
    out->supported      = (sec_word >> 0) & 1;
    out->enabled        = (sec_word >> 1) & 1;
    out->locked          = (sec_word >> 2) & 1;
    out->frozen          = (sec_word >> 3) & 1;
    out->count_expired   = (sec_word >> 4) & 1;
    out->enhanced_erase  = (sec_word >> 5) & 1;
    out->master_pwd_cap  = (sec_word >> 8) & 1;
}

static void query_power_mode(HANDLE hDisk, PowerModeInfo* out) {
    memset(out, 0, sizeof(*out));
    ATA_PASS_THROUGH_DIRECT aptd;
    memset(&aptd, 0, sizeof(aptd));
    aptd.Length             = sizeof(ATA_PASS_THROUGH_DIRECT);
    aptd.AtaFlags           = ATA_FLAGS_DRDY_REQUIRED;
    aptd.DataTransferLength = 0;
    aptd.TimeOutValue       = 5;
    aptd.DataBuffer         = NULL;
    aptd.CurrentTaskFile[6] = 0xE5; /* CHECK POWER MODE */
    DWORD ret = 0;
    if (!DeviceIoControl(hDisk, IOCTL_ATA_PASS_THROUGH_DIRECT,
                         &aptd, sizeof(aptd), &aptd, sizeof(aptd), &ret, NULL)) return;

    out->valid = 1;
    out->mode = aptd.CurrentTaskFile[1]; /* sector count register = power mode */
    switch (out->mode) {
        case 0x00: strncpy(out->mode_name, "Standby", sizeof(out->mode_name)-1); break;
        case 0x40: strncpy(out->mode_name, "NVMe Idle", sizeof(out->mode_name)-1); break;
        case 0x80: strncpy(out->mode_name, "Idle", sizeof(out->mode_name)-1); break;
        case 0xFF: strncpy(out->mode_name, "Active/Idle", sizeof(out->mode_name)-1); break;
        default:   sprintf(out->mode_name, "Unknown (0x%02X)", out->mode); break;
    }
}

static void query_nvme_identify(HANDLE hDisk, NvmeIdentifyInfo* out) {
    memset(out, 0, sizeof(*out));
    BYTE buf[8192];
    memset(buf, 0, sizeof(buf));
    PROTO_QUERY* pq = (PROTO_QUERY*)buf;
    pq->Query.PropertyId = (STORAGE_PROPERTY_ID)StorageDeviceProtocolSpecificProperty;
    pq->Query.QueryType  = PropertyStandardQuery;
    pq->ProtocolType     = ProtocolTypeNvme;
    pq->DataType         = 1; /* NVMeDataTypeIdentify */
    pq->ProtocolDataRequestValue = 1; /* CNS=1 (Identify Controller) */
    pq->ProtocolDataOffset = sizeof(PROTO_QUERY) - sizeof(STORAGE_PROPERTY_QUERY);
    pq->ProtocolDataLength = 4096;

    BYTE out_buf[8192];
    memset(out_buf, 0, sizeof(out_buf));
    DWORD ret = 0;
    if (!DeviceIoControl(hDisk, IOCTL_STORAGE_QUERY_PROPERTY,
                         buf, sizeof(PROTO_QUERY), out_buf, sizeof(out_buf), &ret, NULL)) return;

    BYTE* id = out_buf + sizeof(STORAGE_PROPERTY_QUERY)
             + (sizeof(PROTO_QUERY) - sizeof(STORAGE_PROPERTY_QUERY));
    if (ret < sizeof(PROTO_QUERY) + 256) return;

    out->valid = 1;
    out->vid   = *(UINT16*)(id + 0);
    out->ssvid = *(UINT16*)(id + 2);
    memcpy(out->serial, id + 4, 20);
    for (int i = 19; i >= 0 && out->serial[i] == ' '; i--) out->serial[i] = '\0';
    memcpy(out->model, id + 24, 40);
    for (int i = 39; i >= 0 && out->model[i] == ' '; i--) out->model[i] = '\0';
    memcpy(out->firmware, id + 64, 8);
    for (int i = 7; i >= 0 && out->firmware[i] == ' '; i--) out->firmware[i] = '\0';

    out->max_transfer_sz = id[77]; /* MDTS */
    out->ctrl_id = *(UINT16*)(id + 78);
    out->ver     = *(UINT32*)(id + 80);
    out->ieee_oui[0] = id[73]; out->ieee_oui[1] = id[74]; out->ieee_oui[2] = id[75];
    out->num_namespaces = (UINT8)(*(UINT32*)(id + 516) & 0xFF);
    memcpy(&out->total_cap_bytes, id + 280, 8); /* low 64 bits of TNVMCAP */
    memcpy(&out->unalloc_cap_bytes, id + 296, 8);
}

static void query_nvme_fw_slots(HANDLE hDisk, NvmeFwSlotInfo* out) {
    memset(out, 0, sizeof(*out));
    BYTE buf[4096];
    memset(buf, 0, sizeof(buf));
    PROTO_QUERY* pq = (PROTO_QUERY*)buf;
    pq->Query.PropertyId = (STORAGE_PROPERTY_ID)StorageDeviceProtocolSpecificProperty;
    pq->Query.QueryType  = PropertyStandardQuery;
    pq->ProtocolType     = ProtocolTypeNvme;
    pq->DataType         = NVMeDataTypeLogPage;
    pq->ProtocolDataRequestValue = 0x03; /* Firmware Slot Info */
    pq->ProtocolDataOffset = sizeof(PROTO_QUERY) - sizeof(STORAGE_PROPERTY_QUERY);
    pq->ProtocolDataLength = 512;

    BYTE out_buf[4096];
    memset(out_buf, 0, sizeof(out_buf));
    DWORD ret = 0;
    if (!DeviceIoControl(hDisk, IOCTL_STORAGE_QUERY_PROPERTY,
                         buf, sizeof(PROTO_QUERY), out_buf, sizeof(out_buf), &ret, NULL)) return;

    BYTE* log = out_buf + sizeof(STORAGE_PROPERTY_QUERY)
              + (sizeof(PROTO_QUERY) - sizeof(STORAGE_PROPERTY_QUERY));
    if (ret < sizeof(PROTO_QUERY) + 64) return;

    out->valid = 1;
    UINT8 afi = log[0];
    out->active_slot  = afi & 0x07;
    out->pending_slot = (afi >> 4) & 0x07;
    for (int s = 0; s < MAX_FW_SLOTS; s++) {
        memcpy(out->slot_rev[s], log + 8 + (s * 8), 8);
        for (int i = 7; i >= 0 && out->slot_rev[s][i] == ' '; i--)
            out->slot_rev[s][i] = '\0';
    }
}

static void query_hpa(HANDLE hDisk, HpaInfo* out, AtaIdentifyInfo* ata) {
    memset(out, 0, sizeof(*out));
    if (!ata->valid) return;

    ATA_PASS_THROUGH_DIRECT aptd;
    memset(&aptd, 0, sizeof(aptd));
    aptd.Length             = sizeof(ATA_PASS_THROUGH_DIRECT);
    aptd.AtaFlags           = ATA_FLAGS_DRDY_REQUIRED;
    aptd.DataTransferLength = 0;
    aptd.TimeOutValue       = 5;
    aptd.DataBuffer         = NULL;

    if (ata->lba48_supported) {
        aptd.CurrentTaskFile[6] = 0x27; /* READ NATIVE MAX ADDRESS EXT */
    } else {
        aptd.CurrentTaskFile[6] = 0xF8; /* READ NATIVE MAX ADDRESS */
    }
    DWORD ret = 0;
    if (!DeviceIoControl(hDisk, IOCTL_ATA_PASS_THROUGH_DIRECT,
                         &aptd, sizeof(aptd), &aptd, sizeof(aptd), &ret, NULL)) return;

    out->valid = 1;
    if (ata->lba48_supported) {
        out->native_max_lba = (UINT64)aptd.CurrentTaskFile[1]
                            | ((UINT64)aptd.CurrentTaskFile[2] << 8)
                            | ((UINT64)aptd.CurrentTaskFile[3] << 16)
                            | ((UINT64)aptd.CurrentTaskFile[4] << 24)
                            | ((UINT64)aptd.CurrentTaskFile[5] << 32);
    } else {
        out->native_max_lba = (UINT64)(aptd.CurrentTaskFile[1]
                            | (aptd.CurrentTaskFile[2] << 8)
                            | (aptd.CurrentTaskFile[3] << 16)
                            | ((aptd.CurrentTaskFile[4] & 0x0F) << 24));
    }
    out->current_max_lba = ata->lba48 > 0 ? ata->lba48 : ata->lba28;
    if (out->native_max_lba > out->current_max_lba) {
        out->hpa_active = 1;
        out->hidden_sectors = out->native_max_lba - out->current_max_lba;
    }
}

static void query_dco(HANDLE hDisk, DcoInfo* out, HpaInfo* hpa) {
    memset(out, 0, sizeof(*out));
    BYTE dco_data[512];
    memset(dco_data, 0, sizeof(dco_data));

    ATA_PASS_THROUGH_DIRECT aptd;
    memset(&aptd, 0, sizeof(aptd));
    aptd.Length             = sizeof(ATA_PASS_THROUGH_DIRECT);
    aptd.AtaFlags           = ATA_FLAGS_DATA_IN | ATA_FLAGS_DRDY_REQUIRED;
    aptd.DataTransferLength = 512;
    aptd.TimeOutValue       = 5;
    aptd.DataBuffer         = dco_data;
    aptd.CurrentTaskFile[0] = 0xC2; /* Features = IDENTIFY */
    aptd.CurrentTaskFile[6] = 0xB1; /* DEVICE CONFIGURATION */
    DWORD ret = 0;
    if (!DeviceIoControl(hDisk, IOCTL_ATA_PASS_THROUGH_DIRECT,
                         &aptd, sizeof(aptd), &aptd, sizeof(aptd), &ret, NULL)) return;

    out->valid = 1;
    WORD* w = (WORD*)dco_data;
    out->real_max_lba = (UINT64)w[2] | ((UINT64)w[3] << 16)
                      | ((UINT64)w[4] << 32) | ((UINT64)w[5] << 48);
    if (hpa->valid && out->real_max_lba > hpa->native_max_lba) {
        out->dco_active = 1;
    }
    out->features_disabled = (w[1] != 0xFFFF && w[1] != 0x0000) ? 1 : 0;
}

static void query_sed_opal(HANDLE hDisk, SedOpalInfo* out) {
    memset(out, 0, sizeof(*out));

    BYTE trusted_buf[512];
    memset(trusted_buf, 0, sizeof(trusted_buf));
    ATA_PASS_THROUGH_DIRECT aptd;
    memset(&aptd, 0, sizeof(aptd));
    aptd.Length             = sizeof(ATA_PASS_THROUGH_DIRECT);
    aptd.AtaFlags           = ATA_FLAGS_DATA_IN | ATA_FLAGS_DRDY_REQUIRED;
    aptd.DataTransferLength = 512;
    aptd.TimeOutValue       = 5;
    aptd.DataBuffer         = trusted_buf;
    aptd.CurrentTaskFile[0] = 0x01;
    aptd.CurrentTaskFile[1] = 1;
    aptd.CurrentTaskFile[6] = 0x5C;

    DWORD ret = 0;
    if (!DeviceIoControl(hDisk, IOCTL_ATA_PASS_THROUGH_DIRECT,
                         &aptd, sizeof(aptd), &aptd, sizeof(aptd), &ret, NULL)) {
        return;
    }

    out->valid = 1;
    out->sed_capable = 1;

    UINT32 total_len = 0;
    if (ret >= 4) {
        total_len = *(UINT32*)trusted_buf;
        total_len = ((total_len >> 24) & 0xFF) | ((total_len >> 8) & 0xFF00)
                  | ((total_len << 8) & 0xFF0000) | ((total_len << 24) & 0xFF000000);
    }

    BYTE* ptr = trusted_buf + 48; /* skip header */
    BYTE* end = trusted_buf + (total_len < 512 ? total_len : 512);
    while ((size_t)(end - ptr) >= 4) {
        UINT16 feat_code = (UINT16)((ptr[0] << 8) | ptr[1]);
        UINT16 feat_len  = (UINT16)((ptr[2] << 8) | ptr[3]);
        size_t rem = (size_t)(end - ptr);
        if ((size_t)feat_len > rem - 4) break;
        switch (feat_code) {
            case 0x0001: /* TPer */ break;
            case 0x0002: /* Locking */
                if (feat_len >= 1) out->locked = (ptr[4] >> 2) & 1;
                break;
            case 0x0100: out->enterprise_ssc = 1; break;
            case 0x0200: out->opal_v1 = 1; break;
            case 0x0203: out->opal_v2 = 1; break;
            case 0x0304: out->ruby_ssc = 1; break;
            case 0x0302: out->pyrite_v1 = 1; break;
            case 0x0303: out->pyrite_v2 = 1; break;
        }
        ptr += 4 + feat_len;
    }

    char* p = out->desc;
    size_t cap = sizeof(out->desc);
    size_t n = 0;
    int w;
    if (out->opal_v2)
        w = snprintf(p + n, cap - n, "OPAL v2 ");
    else if (out->opal_v1)
        w = snprintf(p + n, cap - n, "OPAL v1 ");
    else
        w = 0;
    if (w > 0) n += (size_t)w;
    if (out->enterprise_ssc) {
        w = snprintf(p + n, cap - n, "Enterprise ");
        if (w > 0) n += (size_t)w;
    }
    if (out->ruby_ssc) {
        w = snprintf(p + n, cap - n, "Ruby ");
        if (w > 0) n += (size_t)w;
    }
    if (out->pyrite_v1) {
        w = snprintf(p + n, cap - n, "Pyrite v1 ");
        if (w > 0) n += (size_t)w;
    }
    if (out->pyrite_v2) {
        w = snprintf(p + n, cap - n, "Pyrite v2 ");
        if (w > 0) n += (size_t)w;
    }
    if (out->locked) {
        w = snprintf(p + n, cap - n, "[LOCKED]");
        if (w > 0) n += (size_t)w;
    }
    (void)n;
}

typedef struct {
    SCSI_PASS_THROUGH spt;
    BYTE sense[32];
    BYTE data[256];
} SCSI_PTW_BUF;

static void query_scsi_modes(HANDLE hDisk, ScsiModePagesInfo* out) {
    memset(out, 0, sizeof(*out));
    SCSI_PTW_BUF sptwb;
    DWORD data_off  = (DWORD)((BYTE*)&sptwb.data  - (BYTE*)&sptwb);
    DWORD sense_off = (DWORD)((BYTE*)&sptwb.sense - (BYTE*)&sptwb);

    memset(&sptwb, 0, sizeof(sptwb));
    sptwb.spt.Length             = sizeof(SCSI_PASS_THROUGH);
    sptwb.spt.CdbLength          = 6;
    sptwb.spt.SenseInfoLength    = sizeof(sptwb.sense);
    sptwb.spt.DataIn             = 1;
    sptwb.spt.DataTransferLength = sizeof(sptwb.data);
    sptwb.spt.TimeOutValue       = 5;
    sptwb.spt.DataBufferOffset   = data_off;
    sptwb.spt.SenseInfoOffset    = sense_off;
    sptwb.spt.Cdb[0] = 0x1A;
    sptwb.spt.Cdb[2] = 0x08;
    sptwb.spt.Cdb[4] = 0xFF; /* max for MODE SENSE(6) */

    DWORD ret = 0;
    if (DeviceIoControl(hDisk, IOCTL_SCSI_PASS_THROUGH,
                        &sptwb, sizeof(sptwb), &sptwb, sizeof(sptwb), &ret, NULL)) {
        out->valid = 1;
        BYTE* page = sptwb.data + 4;
        if ((page[0] & 0x3F) == 0x08 && ret > 16) {
            out->write_cache = (page[2] >> 2) & 1;
            out->read_cache  = !((page[2] >> 0) & 1);
        }
    }

    memset(&sptwb, 0, sizeof(sptwb));
    sptwb.spt.Length             = sizeof(SCSI_PASS_THROUGH);
    sptwb.spt.CdbLength          = 6;
    sptwb.spt.SenseInfoLength    = sizeof(sptwb.sense);
    sptwb.spt.DataIn             = 1;
    sptwb.spt.DataTransferLength = sizeof(sptwb.data);
    sptwb.spt.TimeOutValue       = 5;
    sptwb.spt.DataBufferOffset   = data_off;
    sptwb.spt.SenseInfoOffset    = sense_off;
    sptwb.spt.Cdb[0] = 0x1A;
    sptwb.spt.Cdb[2] = 0x01;
    sptwb.spt.Cdb[4] = 0xFF; /* max for MODE SENSE(6) */

    if (DeviceIoControl(hDisk, IOCTL_SCSI_PASS_THROUGH,
                        &sptwb, sizeof(sptwb), &sptwb, sizeof(sptwb), &ret, NULL)) {
        BYTE* page = sptwb.data + 4;
        if ((page[0] & 0x3F) == 0x01 && ret > 12) {
            if (!out->valid) out->valid = 1;
            out->awre = (page[2] >> 7) & 1;
            out->arre = (page[2] >> 6) & 1;
            out->error_recovery = page[3];
        }
    }
}

int scan_disks(DiskInfo* disks, int max_disks) {
    int count = 0;
    for (int drv = 0; drv < max_disks; drv++) {
        char path[64];
        sprintf(path, "\\\\.\\PhysicalDrive%d", drv);

        HANDLE hDisk = CreateFileA(path, GENERIC_READ | GENERIC_WRITE,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE,
                                    NULL, OPEN_EXISTING, 0, NULL);
        if (hDisk == INVALID_HANDLE_VALUE) continue;

        DiskInfo* d = &disks[count];
        memset(d, 0, sizeof(DiskInfo));
        d->drive_number = drv;
        sprintf(d->dev_path, "\\\\.\\PhysicalDrive%d", drv);

        query_storage_prop(hDisk, &d->storage);
        query_ata_id(hDisk, &d->ata);
        query_dev_ids(hDisk, &d->ids);
        query_geometry(hDisk, &d->geometry);
        query_smart(hDisk, &d->smart);
        query_smart_thresh(hDisk, &d->smart_thresh, &d->smart);
        query_nvme_health(hDisk, &d->nvme_health);
        query_nvme_identify(hDisk, &d->nvme_id);
        query_nvme_fw_slots(hDisk, &d->nvme_fw);
        query_cache(hDisk, &d->cache);
        query_extra_props(hDisk, &d->extra);
        query_perf(hDisk, &d->perf);
        query_layout(hDisk, &d->layout);
        query_raw_mbr(hDisk, &d->mbr);
        query_ata_security(hDisk, &d->ata_sec);
        query_power_mode(hDisk, &d->power);
        query_hpa(hDisk, &d->hpa, &d->ata);
        query_dco(hDisk, &d->dco, &d->hpa);
        query_sed_opal(hDisk, &d->sed);
        query_scsi_modes(hDisk, &d->scsi_modes);

        CloseHandle(hDisk);
        query_devpath(drv, &d->devpath);
        query_vols(drv, &d->volumes);
        count++;
    }
    return count;
}

#endif /* _WIN32 */
