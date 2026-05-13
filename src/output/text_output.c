#include "../core/disk_info.h"

static const char* part_type_str(BYTE pt) {
    switch (pt) {
        case 0x07: return "NTFS/exFAT";  case 0x0B: return "FAT32 CHS";
        case 0x0C: return "FAT32 LBA";   case 0x0E: return "FAT16 LBA";
        case 0x82: return "Linux Swap";   case 0x83: return "Linux";
        case 0xEE: return "GPT Prot.";   case 0xEF: return "EFI Sys";
        default:   return "";
    }
}

void output_text(DiskInfo* disks, int count) {
    printf("==============================================================\n");
    printf("  Disk Identifier Inspector\n");
    printf("==============================================================\n");

    for (int i = 0; i < count; i++) {
        DiskInfo* d = &disks[i];
        printf("\n##############################################################\n");
        printf("### Physical Drive %d\n", d->drive_number);
        printf("##############################################################\n");

        if (d->storage.valid) {
            print_section("Storage Device Property");
            if (d->storage.vendor[0])  printf("  Vendor:       %s\n", d->storage.vendor);
            if (d->storage.product[0]) printf("  Product:      %s\n", d->storage.product);
            if (d->storage.revision[0])printf("  Revision:     %s\n", d->storage.revision);
            if (d->storage.serial[0])  printf("  Serial:       %s\n", d->storage.serial);
            printf("  Bus Type:     %s (%d)\n", d->storage.bus_name, d->storage.bus_type);
            printf("  Removable:    %s\n", d->storage.removable ? "Yes" : "No");
        }

        if (d->ata.valid) {
            print_section("ATA IDENTIFY DEVICE");
            printf("  Model:        %s\n", d->ata.model);
            printf("  Serial:       %s\n", d->ata.serial);
            printf("  Firmware:     %s\n", d->ata.firmware);
            printf("  ATA Version:  %s (0x%04X)\n", ata_version_name(d->ata.ata_major), d->ata.ata_major);
            printf("  Capacity:     %.1f GB (%llu sectors)\n", d->ata.capacity_gb, (unsigned long long)(d->ata.lba48 ? d->ata.lba48 : d->ata.lba28));
            if (d->ata.wwn) {
                printf("  WWN:          %016llX\n", (unsigned long long)d->ata.wwn);
                printf("  NAA / OUI:    %u / %06X\n", d->ata.naa, d->ata.oui);
            }
            if (d->ata.is_ssd) printf("  Media:        SSD\n");
            else if (d->ata.rpm) printf("  RPM:          %u\n", d->ata.rpm);
            if (d->ata.form_factor) printf("  Form Factor:  %s\n", form_factor_name(d->ata.form_factor));
            if (d->ata.cache_kb) printf("  Cache:        %u KB\n", d->ata.cache_kb);
            printf("  Sector Size:  %u logical, %ux physical\n", d->ata.log_sector_size, d->ata.phys_log_ratio);
            printf("  SATA Gen:     %d (%.1f Gbps)\n", d->ata.sata_gen, d->ata.sata_gen * 1.5);
            printf("  NCQ:          %s (depth %u)\n", d->ata.ncq_supported ? "Yes" : "No", d->ata.queue_depth);
            printf("  TRIM:         %s\n", d->ata.trim_supported ? "Yes" : "No");
            printf("  Write Cache:  %s\n", d->ata.write_cache ? "Enabled" : "Disabled");
            printf("  48-bit LBA:   %s\n", d->ata.lba48_supported ? "Yes" : "No");
            printf("  SMART:        %s\n", d->ata.smart_enabled ? "Enabled" : (d->ata.smart_supported ? "Supported" : "No"));
        }

        if (d->geometry.valid) {
            print_section("Disk Geometry");
            printf("  Total Size:   %.2f GB (%llu bytes)\n", (double)d->geometry.total_bytes / 1e9, (unsigned long long)d->geometry.total_bytes);
            printf("  Bytes/Sector: %lu\n", (unsigned long)d->geometry.bytes_per_sector);
            printf("  CHS:          %llu / %lu / %lu\n", (unsigned long long)d->geometry.cylinders, (unsigned long)d->geometry.heads, (unsigned long)d->geometry.sectors_per_track);
        }

        if (d->smart.valid) {
            print_section("SMART Health");
            printf("  Status:       %s\n", d->smart.health_ok ? "PASSED" : "FAILED");
            if (d->smart.has_temp) printf("  Temperature:  %u C\n", d->smart.temperature);
            if (d->smart.has_poh) printf("  Power-On Hrs: %lu\n", (unsigned long)d->smart.power_on_hours);
            if (d->smart.has_power_cycles) printf("  Power Cycles: %lu\n", (unsigned long)d->smart.power_cycles);
            if (d->smart.has_reallocated) printf("  Realloc Secs: %lu\n", (unsigned long)d->smart.reallocated_sectors);
            printf("  Attributes:   %d total\n", d->smart.attr_count);
            for (int j = 0; j < d->smart.attr_count; j++) {
                SmartAttr* a = &d->smart.attrs[j];
                printf("    0x%02X %-24s cur=%3u wst=%3u raw=%llu\n",
                       a->id, a->name, a->current, a->worst, (unsigned long long)a->raw);
            }
        }

        if (d->ids.valid && d->ids.count > 0) {
            print_section("VPD Page 83 Identifiers");
            for (int j = 0; j < d->ids.count; j++) {
                VpdIdentifier* e = &d->ids.entries[j];
                printf("  [%d] %-16s %s\n", j, e->type_name, e->is_ascii ? e->data_ascii : e->data_hex);
            }
        }

        if (d->layout.valid) {
            print_section("Drive Layout");
            if (d->layout.style == 0) {
                printf("  Style:        MBR\n");
                printf("  MBR Sig:      0x%08lX\n", (unsigned long)d->layout.mbr_signature);
            } else if (d->layout.style == 1) {
                printf("  Style:        GPT\n");
                printf("  GPT GUID:     %s\n", d->layout.gpt_guid);
            }
            for (int p = 0; p < d->layout.detail_count; p++) {
                PartitionInfo* pi = &d->layout.parts[p];
                printf("  Part %u:  %.1f GB  offset=%llu", pi->number,
                       (double)pi->length / 1e9, (unsigned long long)pi->offset);
                if (pi->is_gpt && pi->gpt_name[0]) printf("  \"%s\"", pi->gpt_name);
                else if (!pi->is_gpt) printf("  type=0x%02X (%s)", pi->mbr_type, part_type_str(pi->mbr_type));
                printf("\n");
            }
        }

        if (d->mbr.valid) {
            print_section("Raw MBR");
            printf("  Disk Sig:     0x%08X\n", d->mbr.disk_sig);
            printf("  Boot Sig:     0x%04X %s\n", d->mbr.boot_sig, d->mbr.boot_sig == 0xAA55 ? "(valid)" : "(INVALID)");
        }

        if (d->nvme_health.valid) {
            print_section("NVMe Health Log");
            int temp_c = (int)d->nvme_health.temperature - 273;
            printf("  Critical Warn: 0x%02X\n", d->nvme_health.critical_warning);
            printf("  Temperature:   %d C (%u K)\n", temp_c, d->nvme_health.temperature);
            printf("  Avail Spare:   %u%% (threshold %u%%)\n", d->nvme_health.avail_spare, d->nvme_health.avail_spare_thresh);
            printf("  Percent Used:  %u%%\n", d->nvme_health.percent_used);
            printf("  Data Read:     %.2f TB\n", (double)d->nvme_health.data_units_read * 512000.0 / 1e12);
            printf("  Data Written:  %.2f TB\n", (double)d->nvme_health.data_units_written * 512000.0 / 1e12);
            printf("  Power-On Hrs:  %llu\n", (unsigned long long)d->nvme_health.power_on_hours);
            printf("  Unsafe Shutdn: %llu\n", (unsigned long long)d->nvme_health.unsafe_shutdowns);
            printf("  Media Errors:  %llu\n", (unsigned long long)d->nvme_health.media_errors);
        }

        if (d->cache.valid) {
            print_section("Cache Policy");
            printf("  Read Cache:    %s\n", d->cache.read_cache ? "Enabled" : "Disabled");
            printf("  Write Cache:   %s\n", d->cache.write_cache ? "Enabled" : "Disabled");
            printf("  Write Mode:    %s\n", d->cache.write_through ? "WriteThrough" : "WriteBack");
            printf("  Power Protect: %s\n", d->cache.power_protected ? "Yes" : "No");
        }

        if (d->extra.valid) {
            print_section("Extra Properties");
            printf("  Seek Penalty:  %s (%s)\n", d->extra.has_seek_penalty ? "Yes" : "No",
                   d->extra.has_seek_penalty ? "HDD" : "SSD");
            printf("  TRIM (OS):     %s\n", d->extra.trim_enabled ? "Enabled" : "Disabled");
            if (d->extra.align_valid) printf("  Align Offset:  %lu bytes\n", (unsigned long)d->extra.align_byte_offset);
        }

        if (d->perf.valid) {
            print_section("Performance Counters");
            printf("  Reads:         %llu (%.2f GB)\n", (unsigned long long)d->perf.read_count, (double)d->perf.bytes_read / 1e9);
            printf("  Writes:        %llu (%.2f GB)\n", (unsigned long long)d->perf.write_count, (double)d->perf.bytes_written / 1e9);
            printf("  Queue Depth:   %lu\n", (unsigned long)d->perf.queue_depth);
        }

        if (d->smart_thresh.valid && d->smart_thresh.count > 0) {
            print_section("SMART Thresholds");
            for (int j = 0; j < d->smart_thresh.count; j++) {
                SmartThreshold* t = &d->smart_thresh.entries[j];
                printf("  ID %3u: Threshold=%3u %s\n", t->id, t->threshold,
                       t->exceeded ? "** EXCEEDED **" : "OK");
            }
        }

        if (d->nvme_id.valid) {
            print_section("NVMe Identify Controller");
            printf("  Serial:        %s\n", d->nvme_id.serial);
            printf("  Model:         %s\n", d->nvme_id.model);
            printf("  Firmware:      %s\n", d->nvme_id.firmware);
            printf("  PCI VID:       0x%04X\n", d->nvme_id.vid);
            printf("  Subsys VID:    0x%04X\n", d->nvme_id.ssvid);
            printf("  Controller ID: %u\n", d->nvme_id.ctrl_id);
            printf("  NVMe Version:  %u.%u.%u\n", (d->nvme_id.ver>>16)&0xFF, (d->nvme_id.ver>>8)&0xFF, d->nvme_id.ver&0xFF);
            printf("  IEEE OUI:      %02X:%02X:%02X\n", d->nvme_id.ieee_oui[0], d->nvme_id.ieee_oui[1], d->nvme_id.ieee_oui[2]);
            printf("  Namespaces:    %u\n", d->nvme_id.num_namespaces);
            printf("  Total NVM:     %.2f GB\n", (double)d->nvme_id.total_cap_bytes / 1e9);
            printf("  MDTS:          %u\n", d->nvme_id.max_transfer_sz);
        }

        if (d->nvme_fw.valid) {
            print_section("NVMe Firmware Slots");
            printf("  Active Slot:   %u\n", d->nvme_fw.active_slot);
            if (d->nvme_fw.pending_slot) printf("  Pending Slot:  %u\n", d->nvme_fw.pending_slot);
            for (int s = 0; s < MAX_FW_SLOTS; s++) {
                if (d->nvme_fw.slot_rev[s][0])
                    printf("  Slot %d:        %s%s\n", s+1, d->nvme_fw.slot_rev[s],
                           (s+1 == d->nvme_fw.active_slot) ? " [ACTIVE]" : "");
            }
        }

        if (d->ata_sec.valid) {
            print_section("ATA Security");
            printf("  Supported:     %s\n", d->ata_sec.supported ? "Yes" : "No");
            if (d->ata_sec.supported) {
                printf("  Enabled:       %s\n", d->ata_sec.enabled ? "Yes" : "No");
                printf("  Locked:        %s\n", d->ata_sec.locked ? "YES" : "No");
                printf("  Frozen:        %s\n", d->ata_sec.frozen ? "Yes" : "No");
                printf("  Enh. Erase:    %s\n", d->ata_sec.enhanced_erase ? "Yes" : "No");
                printf("  Master Pwd:    %s\n", d->ata_sec.master_pwd_cap ? "Maximum" : "High");
            }
        }

        if (d->power.valid) {
            print_section("Power Mode");
            printf("  State:         %s (0x%02X)\n", d->power.mode_name, d->power.mode);
        }

        if (d->hpa.valid) {
            print_section("Host Protected Area");
            printf("  Native Max:    %llu LBA\n", (unsigned long long)d->hpa.native_max_lba);
            printf("  Current Max:   %llu LBA\n", (unsigned long long)d->hpa.current_max_lba);
            printf("  HPA Active:    %s\n", d->hpa.hpa_active ? "YES" : "No");
            if (d->hpa.hpa_active) printf("  Hidden:        %llu sectors\n", (unsigned long long)d->hpa.hidden_sectors);
        }

        if (d->dco.valid) {
            print_section("Device Configuration Overlay");
            printf("  Real Max LBA:  %llu\n", (unsigned long long)d->dco.real_max_lba);
            printf("  DCO Active:    %s\n", d->dco.dco_active ? "YES" : "No");
            printf("  Features Hidden: %s\n", d->dco.features_disabled ? "Yes" : "No");
        }

        if (d->sed.valid) {
            print_section("Self-Encrypting Drive");
            printf("  SED Capable:   %s\n", d->sed.sed_capable ? "Yes" : "No");
            if (d->sed.desc[0]) printf("  Standard:      %s\n", d->sed.desc);
            printf("  Locked:        %s\n", d->sed.locked ? "YES" : "No");
        }

        if (d->scsi_modes.valid) {
            print_section("SCSI Mode Pages");
            printf("  Write Cache:   %s\n", d->scsi_modes.write_cache ? "Enabled" : "Disabled");
            printf("  Read Cache:    %s\n", d->scsi_modes.read_cache ? "Enabled" : "Disabled");
            printf("  Auto Write Realloc: %s\n", d->scsi_modes.awre ? "Yes" : "No");
            printf("  Auto Read Realloc:  %s\n", d->scsi_modes.arre ? "Yes" : "No");
            printf("  Read Retry:    %u\n", d->scsi_modes.error_recovery);
        }

        if (d->devpath.valid) {
            print_section("Device Path");
            if (d->devpath.device_path[0]) printf("  Instance:      %s\n", d->devpath.device_path);
            if (d->devpath.friendly_name[0]) printf("  Friendly:      %s\n", d->devpath.friendly_name);
            if (d->devpath.hw_id[0]) printf("  Hardware ID:   %s\n", d->devpath.hw_id);
            if (d->devpath.location[0]) printf("  Location:      %s\n", d->devpath.location);
        }

        if (d->volumes.count > 0) {
            print_section("Volumes");
            for (int j = 0; j < d->volumes.count; j++) {
                VolumeInfo* v = &d->volumes.vols[j];
                if (v->letter) {
                    printf("  %c:\\  Serial=0x%08lX  FS=%-6s Label=\"%s\"\n",
                           v->letter, (unsigned long)v->serial, v->fs_name, v->label);
                    if (v->guid_path[0]) printf("       %s\n", v->guid_path);
                } else if (v->mount_point[0]) {
                    printf("  %-12s FS=%-6s\n", v->mount_point, v->fs_name);
                }
            }
        }
    }

    printf("\n==============================================================\n");
    printf("  Done. Found %d physical drive(s).\n", count);
    printf("==============================================================\n");
}
