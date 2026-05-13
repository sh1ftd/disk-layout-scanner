#ifndef _WIN32
#include "../core/disk_info.h"

#ifndef NVME_LOG_PAGE_HEALTH_INFO
#define NVME_LOG_PAGE_HEALTH_INFO 0x02
#endif

static int read_sysfs(const char* path, char* buf, int sz) {
    FILE* f = fopen(path, "r");
    if (!f) return 0;
    buf[0] = '\0';
    if (!fgets(buf, sz, f)) {
        fclose(f);
        return 0;
    }
    int len = (int)strlen(buf);
    while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == ' '))
        buf[--len] = '\0';
    fclose(f);
    return 1;
}

static long long read_sysfs_ll(const char* path) {
    char buf[64];
    if (!read_sysfs(path, buf, sizeof(buf))) return -1;
    return atoll(buf);
}

static void parse_ata_identify(const BYTE* identify, AtaIdentifyInfo* out) {
    out->valid = 1;
    uint16_t w[256];
    memcpy(w, identify, sizeof(w));

    ata_words_to_string((const WORD*)w, 10, 19, out->serial, sizeof(out->serial));
    ata_words_to_string((const WORD*)w, 23, 26, out->firmware, sizeof(out->firmware));
    ata_words_to_string((const WORD*)w, 27, 46, out->model, sizeof(out->model));

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
    out->trim_supported   = (w[169] >> 0) & 1;

    if (w[106] & (1 << 14)) {
        out->phys_log_ratio = 1 << (w[106] & 0x0F);
        out->log_sector_size = (w[106] & (1 << 12)) ?
            (UINT16)(((UINT32)w[117] | ((UINT32)w[118] << 16)) * 2) : 512;
    } else {
        out->log_sector_size = 512;
        out->phys_log_ratio = 1;
    }
    out->form_factor = (UINT8)(w[168] & 0x0F);
    out->ata_major = w[80];
    if (w[88] & 0x7F00) {
        UINT8 active = (w[88] >> 8) & 0x7F;
        for (int b = 6; b >= 0; b--)
            if (active & (1 << b)) { out->udma_mode = (UINT8)b; break; }
    }
    out->transport_major = w[222];
}

static void parse_smart_data(const BYTE* data, SmartInfo* out) {
    out->valid = 1;
    out->health_ok = 1;

    for (int i = 0; i < 30 && out->attr_count < MAX_SMART_ATTRS; i++) {
        const BYTE* entry = data + 2 + (i * 12);
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

        if (id == 0xC2 || id == 0xBE) { out->has_temp = 1; out->temperature = (UINT8)(a->raw & 0xFF); }
        if (id == 0x09) { out->has_poh = 1; out->power_on_hours = (UINT32)(a->raw & 0xFFFFFFFF); }
        if (id == 0x0C) { out->has_power_cycles = 1; out->power_cycles = (UINT32)(a->raw & 0xFFFFFFFF); }
        if (id == 0x05) { out->has_reallocated = 1; out->reallocated_sectors = (UINT32)(a->raw & 0xFFFFFFFF); }

        out->attr_count++;
    }
}

static void scan_sata_disk(const char* dev_path, const char* name, DiskInfo* d) {
    int fd = open(dev_path, O_RDONLY | O_NONBLOCK);
    if (fd < 0) return;

    d->storage.valid = 1;
    char syspath[512];
    sprintf(syspath, "/sys/block/%s/device/vendor", name);
    read_sysfs(syspath, d->storage.vendor, sizeof(d->storage.vendor));
    sprintf(syspath, "/sys/block/%s/device/model", name);
    read_sysfs(syspath, d->storage.product, sizeof(d->storage.product));
    sprintf(syspath, "/sys/block/%s/device/rev", name);
    read_sysfs(syspath, d->storage.revision, sizeof(d->storage.revision));

    char transport[64] = {0};
    sprintf(syspath, "/sys/block/%s/device/transport", name);
    if (read_sysfs(syspath, transport, sizeof(transport))) {
        if (strstr(transport, "sata")) d->storage.bus_type = 11;
        else if (strstr(transport, "sas")) d->storage.bus_type = 10;
        else if (strstr(transport, "usb")) d->storage.bus_type = 7;
    } else {
        d->storage.bus_type = 11;
    }
    strncpy(d->storage.bus_name, bus_type_name(d->storage.bus_type), sizeof(d->storage.bus_name)-1);

    BYTE identify[512];
    memset(identify, 0, sizeof(identify));
    if (ioctl(fd, HDIO_GET_IDENTITY, identify) == 0) {
        parse_ata_identify(identify, &d->ata);
        if (d->storage.serial[0] == 0 && d->ata.serial[0])
            strncpy(d->storage.serial, d->ata.serial, sizeof(d->storage.serial)-1);
    }

    {
        BYTE cdb[12] = {0};
        BYTE sense[32] = {0};
        BYTE smart_buf[512] = {0};

        cdb[0] = 0xA1;
        cdb[1] = (4 << 1);
        cdb[2] = 0x2E;
        cdb[3] = SMART_READ_VALUES;
        cdb[4] = 1;
        cdb[8] = 0x4F;
        cdb[9] = 0xC2;
        cdb[11] = SMART_CMD;

        struct sg_io_hdr io;
        memset(&io, 0, sizeof(io));
        io.interface_id = 'S';
        io.dxfer_direction = SG_DXFER_FROM_DEV;
        io.cmd_len = 12;
        io.mx_sb_len = sizeof(sense);
        io.dxfer_len = 512;
        io.dxferp = smart_buf;
        io.cmdp = cdb;
        io.sbp = sense;
        io.timeout = 5000;

        if (ioctl(fd, SG_IO, &io) == 0 && io.status == 0) {
            parse_smart_data(smart_buf, &d->smart);
        }
    }

    sprintf(syspath, "/sys/block/%s/size", name);
    long long sectors = read_sysfs_ll(syspath);
    if (sectors > 0) {
        d->geometry.valid = 1;
        d->geometry.bytes_per_sector = 512;
        sprintf(syspath, "/sys/block/%s/queue/hw_sector_size", name);
        long long hw_ss = read_sysfs_ll(syspath);
        if (hw_ss > 0) d->geometry.bytes_per_sector = (UINT32)hw_ss;
        /* /sys/block/<dev>/size is always in 512-byte sectors, independent of hw_sector_size. */
        d->geometry.total_bytes = (UINT64)sectors * 512;
    }

    sprintf(syspath, "/sys/block/%s/queue/rotational", name);
    long long rot = read_sysfs_ll(syspath);
    d->extra.valid = 1;
    d->extra.has_seek_penalty = (rot == 1) ? 1 : 0;

    sprintf(syspath, "/sys/block/%s/queue/discard_max_bytes", name);
    long long discard = read_sysfs_ll(syspath);
    d->extra.trim_enabled = (discard > 0) ? 1 : 0;

    {
        BYTE mbr[512];
        if (pread(fd, mbr, 512, 0) == 512) {
            d->mbr.valid = 1;
            memcpy(&d->mbr.disk_sig, mbr + 0x1B8, 4);
            memcpy(&d->mbr.boot_sig, mbr + 0x1FE, 2);
            d->mbr.part1_type = mbr[0x1C2];
        }
    }

    d->devpath.valid = 1;
    strncpy(d->devpath.device_path, dev_path, sizeof(d->devpath.device_path)-1);
    sprintf(syspath, "/sys/block/%s/device/driver", name);
    char link[512] = {0};
    ssize_t ll = readlink(syspath, link, sizeof(link)-1);
    if (ll > 0) {
        link[ll] = '\0';
        char* base = strrchr(link, '/');
        if (base) strncpy(d->devpath.driver, base+1, sizeof(d->devpath.driver)-1);
    }

    close(fd);
}

static void scan_nvme_disk(const char* dev_path, const char* name, DiskInfo* d) {
    int fd = open(dev_path, O_RDONLY);
    if (fd < 0) return;

    d->storage.valid = 1;
    d->storage.bus_type = 17;
    strncpy(d->storage.bus_name, "NVMe", sizeof(d->storage.bus_name)-1);

    char syspath[512];
    sprintf(syspath, "/sys/block/%s/device/model", name);
    read_sysfs(syspath, d->storage.product, sizeof(d->storage.product));
    sprintf(syspath, "/sys/block/%s/device/serial", name);
    read_sysfs(syspath, d->storage.serial, sizeof(d->storage.serial));
    sprintf(syspath, "/sys/block/%s/device/firmware_rev", name);
    read_sysfs(syspath, d->storage.revision, sizeof(d->storage.revision));

    sprintf(syspath, "/sys/block/%s/size", name);
    long long sectors = read_sysfs_ll(syspath);
    if (sectors > 0) {
        d->geometry.valid = 1;
        d->geometry.bytes_per_sector = 512;
        /* /sys/block/<dev>/size is always in 512-byte sectors, independent of hw_sector_size. */
        d->geometry.total_bytes = (UINT64)sectors * 512;
    }

    {
        BYTE log[512];
        memset(log, 0, sizeof(log));
        struct nvme_admin_cmd cmd;
        memset(&cmd, 0, sizeof(cmd));
        cmd.opcode = 0x02;
        cmd.nsid = 0xFFFFFFFF;
        cmd.addr = (unsigned long long)(uintptr_t)log;
        cmd.data_len = 512;
        cmd.cdw10 = (NVME_LOG_PAGE_HEALTH_INFO) | (((512 / 4) - 1) << 16);

        if (ioctl(fd, NVME_IOCTL_ADMIN_CMD, &cmd) == 0) {
            d->nvme_health.valid = 1;
            d->nvme_health.critical_warning = log[0];
            d->nvme_health.temperature = (UINT16)(log[1] | (log[2] << 8));
            d->nvme_health.avail_spare = log[3];
            d->nvme_health.avail_spare_thresh = log[4];
            d->nvme_health.percent_used = log[5];
            memcpy(&d->nvme_health.data_units_read, log + 32, 8);
            memcpy(&d->nvme_health.data_units_written, log + 48, 8);
            memcpy(&d->nvme_health.host_read_cmds, log + 64, 8);
            memcpy(&d->nvme_health.host_write_cmds, log + 80, 8);
            memcpy(&d->nvme_health.power_on_hours, log + 128, 8);
            memcpy(&d->nvme_health.unsafe_shutdowns, log + 144, 8);
            memcpy(&d->nvme_health.media_errors, log + 160, 8);
            memcpy(&d->nvme_health.error_log_entries, log + 176, 8);
        }
    }

    d->extra.valid = 1;
    d->extra.has_seek_penalty = 0;
    sprintf(syspath, "/sys/block/%s/queue/discard_max_bytes", name);
    long long discard = read_sysfs_ll(syspath);
    d->extra.trim_enabled = (discard > 0) ? 1 : 0;

    d->devpath.valid = 1;
    strncpy(d->devpath.device_path, dev_path, sizeof(d->devpath.device_path)-1);

    close(fd);
}

static void scan_mounts(const char* disk_name, VolumesInfo* vols) {
    memset(vols, 0, sizeof(*vols));
    FILE* f = fopen("/proc/mounts", "r");
    if (!f) return;

    char line[1024];
    while (fgets(line, sizeof(line), f) && vols->count < MAX_VOLUMES) {
        char dev[256], mount[256], fs[64];
        if (sscanf(line, "%255s %255s %63s", dev, mount, fs) != 3) continue;
        if (strstr(dev, disk_name) == NULL) continue;

        VolumeInfo* v = &vols->vols[vols->count];
        snprintf(v->fs_name, sizeof(v->fs_name), "%s", fs);
        snprintf(v->mount_point, sizeof(v->mount_point), "%s", mount);
        vols->count++;
    }
    fclose(f);
}

static void scan_diskstats(const char* name, PerfInfo* perf) {
    memset(perf, 0, sizeof(*perf));
    FILE* f = fopen("/proc/diskstats", "r");
    if (!f) return;

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        unsigned long long rd_ios, rd_merges, rd_sectors, rd_ticks;
        unsigned long long wr_ios, wr_merges, wr_sectors, wr_ticks;
        unsigned long long io_inflight, io_ticks, io_weighted;
        char dev[64];
        unsigned int major, minor;

        int n = sscanf(line, " %u %u %63s %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
                       &major, &minor, dev,
                       &rd_ios, &rd_merges, &rd_sectors, &rd_ticks,
                       &wr_ios, &wr_merges, &wr_sectors, &wr_ticks,
                       &io_inflight, &io_ticks, &io_weighted);
        if (n < 14) continue;
        if (strcmp(dev, name) != 0) continue;

        perf->valid = 1;
        perf->read_count = rd_ios;
        perf->write_count = wr_ios;
        perf->bytes_read = rd_sectors * 512;
        perf->bytes_written = wr_sectors * 512;
        perf->read_time_ns = rd_ticks * 1000000ULL;
        perf->write_time_ns = wr_ticks * 1000000ULL;
        break;
    }
    fclose(f);
}

static int is_disk_device(const char* name) {
    if (strncmp(name, "sd", 2) == 0 && isalpha(name[2])) return 1;
    if (strncmp(name, "vd", 2) == 0 && isalpha(name[2])) return 1;
    if (strncmp(name, "xvd", 3) == 0 && isalpha(name[3])) return 1;
    if (strncmp(name, "hd", 2) == 0 && isalpha(name[2])) return 1;
    if (strncmp(name, "nvme", 4) == 0) {
        const char* p = name + 4;
        while (isdigit(*p)) p++;
        if (*p == 'n') { p++; while (isdigit(*p)) p++; }
        return (*p == '\0');
    }
    return 0;
}

static int is_nvme(const char* name) {
    return strncmp(name, "nvme", 4) == 0;
}

int scan_disks(DiskInfo* disks, int max_disks) {
    int count = 0;
    DIR* dir = opendir("/sys/block");
    if (!dir) return 0;

    struct dirent* ent;
    while ((ent = readdir(dir)) != NULL && count < max_disks) {
        if (!is_disk_device(ent->d_name)) continue;

        char dev[64];
        snprintf(dev, sizeof(dev), "/dev/%s", ent->d_name);

        DiskInfo* d = &disks[count];
        memset(d, 0, sizeof(DiskInfo));
        d->drive_number = count;
        snprintf(d->dev_path, sizeof(d->dev_path), "%s", dev);

        if (is_nvme(ent->d_name))
            scan_nvme_disk(dev, ent->d_name, d);
        else
            scan_sata_disk(dev, ent->d_name, d);

        scan_diskstats(ent->d_name, &d->perf);
        scan_mounts(ent->d_name, &d->volumes);
        count++;
    }
    closedir(dir);

    return count;
}

#endif
