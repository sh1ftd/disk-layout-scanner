#include "../core/disk_info.h"

static void row(FILE* f, const char* l, const char* cls, const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    fprintf(f, "<div class=\"row\"><span class=\"lbl\">%s</span><span class=\"val %s\">", l, cls);
    vfprintf(f, fmt, ap);
    fputs("</span></div>\n", f);
    va_end(ap);
}

static void sec_open(FILE* f, const char* title) {
    fprintf(f, "<details class=\"sec\" open><summary>%s</summary><div class=\"sec-body\">\n", title);
}

static void sec_close(FILE* f) {
    fputs("</div></details>\n", f);
}

int output_html(DiskInfo* disks, int count, const char* filename) {
    FILE* f = fopen(filename, "wb");
    if (!f) {
        fprintf(stderr, "disk-layout-scanner: cannot write %s\n", filename);
        return -1;
    }

    fputs("<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\">\n"
          "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">\n"
          "<title>disk-layout-scanner</title>\n<style>\n", f);
    fputs(
        "*{margin:0;padding:0;box-sizing:border-box}\n"
        "body{font-family:'Segoe UI',system-ui,sans-serif;background:#0d1117;color:#e6edf3;padding:24px}\n"
        "h1{text-align:center;font-size:1.8rem;margin-bottom:8px;background:linear-gradient(135deg,#58a6ff,#bc8cff);-webkit-background-clip:text;-webkit-text-fill-color:transparent}\n"
        ".sub{text-align:center;color:#8b949e;margin-bottom:32px;font-size:.9rem}\n"
        ".grid{display:grid;grid-template-columns:repeat(auto-fill,minmax(520px,1fr));gap:20px;max-width:1400px;margin:0 auto}\n"
        ".card{background:#161b22;border:1px solid #30363d;border-radius:12px;overflow:hidden;transition:border-color .2s}\n"
        ".card:hover{border-color:#58a6ff}\n"
        ".card>summary.card-hdr{padding:14px 20px;background:#1c2129;border-bottom:1px solid #30363d;display:flex;justify-content:space-between;align-items:center;cursor:pointer;list-style:none;user-select:none}\n"
        ".card>summary.card-hdr::before{content:'\\25BC';font-size:.6rem;margin-right:10px;transition:transform .2s;display:inline-block;color:#8b949e}\n"
        ".card:not([open])>summary.card-hdr::before{transform:rotate(-90deg)}\n"
        ".card>summary.card-hdr:hover::before{color:#58a6ff}\n"
        ".card-hdr h2{font-size:1.05rem;font-weight:600}\n"
        ".badge{font-size:.7rem;padding:3px 10px;border-radius:12px;font-weight:600}\n"
        ".b-sata{background:#1f3a2d;color:#3fb950}.b-nvme{background:#1f2d3a;color:#58a6ff}.b-other{background:#2d2a1f;color:#d29922}\n"
        "details.sec{padding:0;border-bottom:1px solid #21262d}\n"
        "details.sec:last-child{border-bottom:none}\n"
        "details.sec>summary{padding:10px 20px;cursor:pointer;font-size:.7rem;text-transform:uppercase;letter-spacing:1px;color:#8b949e;list-style:none;display:flex;align-items:center;user-select:none;transition:color .2s}\n"
        "details.sec>summary:hover{color:#58a6ff}\n"
        "details.sec>summary::before{content:'\\25B6';font-size:.6rem;margin-right:8px;transition:transform .2s;display:inline-block}\n"
        "details.sec[open]>summary::before{transform:rotate(90deg)}\n"
        "details.sec>.sec-body{padding:2px 20px 10px}\n"
        ".row{display:flex;padding:1px 0;font-size:.82rem}\n"
        ".lbl{color:#8b949e;min-width:125px;flex-shrink:0}\n"
        ".val{color:#e6edf3;word-break:break-all;font-family:'Cascadia Code','Fira Code',monospace;font-size:.8rem}\n"
        ".hl{color:#58a6ff}.wwn{color:#bc8cff}.vol{color:#3fb950}.warn{color:#f85149}\n"
        ".pill{display:inline-block;font-size:.7rem;padding:1px 8px;border-radius:8px;margin-left:6px}\n"
        ".p-on{background:#1f3a2d;color:#3fb950}.p-off{background:#3d1f20;color:#f85149}\n"
        ".id-e{background:#0d1117;border-radius:6px;padding:5px 10px;margin:3px 0;font-size:.78rem;font-family:monospace}\n"
        ".smart-grid{display:grid;grid-template-columns:auto 1fr auto auto auto;gap:0 12px;font-size:.78rem;font-family:monospace}\n"
        ".smart-grid .sh{color:#8b949e;font-size:.7rem;text-transform:uppercase;border-bottom:1px solid #21262d;padding-bottom:3px;margin-bottom:3px}\n"
        ".part-row{font-size:.78rem;padding:2px 0;font-family:monospace;color:#8b949e}\n"
        ".footer{text-align:center;color:#484f58;margin-top:32px;font-size:.8rem}\n"
        ".info-bar{text-align:center;color:#8b949e;margin-bottom:24px;padding:12px;background:#161b22;border-radius:8px;max-width:700px;margin-left:auto;margin-right:auto}\n"
    , f);
    fputs("</style></head><body>\n", f);

    fputs("<h1>disk-layout-scanner</h1>\n<p class=\"sub\">Cross-platform layout and identity reporting</p>\n", f);
    int sc = 0, nc = 0;
    for (int i = 0; i < count; i++) { if (disks[i].storage.bus_type == 11) sc++; else if (disks[i].storage.bus_type == 17) nc++; }
    fprintf(f, "<div class=\"info-bar\"><strong>%d</strong> drive(s) &mdash; ", count);
    if (sc) fprintf(f, "%d SATA ", sc);
    if (sc && nc) fputs("/ ", f);
    if (nc) fprintf(f, "%d NVMe", nc);
    fputs("</div>\n<div class=\"grid\">\n", f);

    for (int i = 0; i < count; i++) {
        DiskInfo* d = &disks[i];
        const char* bc = d->storage.bus_type == 11 ? "b-sata" : d->storage.bus_type == 17 ? "b-nvme" : "b-other";
        const char* prod = d->storage.product[0] ? d->storage.product : "Unknown";

        fprintf(f, "<details class=\"card\" open><summary class=\"card-hdr\"><h2>Drive %d &mdash; %s</h2>"
                   "<span class=\"badge %s\">%s</span></summary>\n", d->drive_number, prod, bc, d->storage.bus_name);

        if (d->storage.valid) {
            sec_open(f, "Storage Property");
            if (d->storage.vendor[0]) row(f, "Vendor", "", "%s", d->storage.vendor);
            row(f, "Product", "", "%s", d->storage.product);
            if (d->storage.revision[0]) row(f, "Revision", "", "%s", d->storage.revision);
            if (d->storage.serial[0]) row(f, "Serial", "hl", "%s", d->storage.serial);
            sec_close(f);
        }

        if (d->ata.valid) {
            sec_open(f, "ATA Identify Device");
            row(f, "Model", "", "%s", d->ata.model);
            row(f, "Serial", "hl", "%s", d->ata.serial);
            row(f, "Firmware", "", "%s", d->ata.firmware);
            row(f, "ATA Version", "", "%s", ata_version_name(d->ata.ata_major));
            row(f, "Capacity", "", "%.1f GB", d->ata.capacity_gb);
            if (d->ata.wwn) {
                fprintf(f, "<div class=\"row\"><span class=\"lbl\">WWN</span><span class=\"val wwn\">%016llX</span></div>\n", (unsigned long long)d->ata.wwn);
                fprintf(f, "<div class=\"row\"><span class=\"lbl\">NAA / OUI</span><span class=\"val wwn\">%u / %06X</span></div>\n", d->ata.naa, d->ata.oui);
            }
            if (d->ata.form_factor) row(f, "Form Factor", "", "%s", form_factor_name(d->ata.form_factor));
            fprintf(f, "<div class=\"row\"><span class=\"lbl\">Media</span><span class=\"val\">%s</span></div>\n",
                    d->ata.is_ssd ? "SSD" : (d->ata.rpm ? "HDD" : "Unknown"));
            if (d->ata.rpm) row(f, "RPM", "", "%u", d->ata.rpm);
            if (d->ata.cache_kb) row(f, "Cache", "", "%u KB", d->ata.cache_kb);
            row(f, "Sector Size", "", "%u log / %ux phys", d->ata.log_sector_size, d->ata.phys_log_ratio);
            if (d->ata.sata_gen)
                row(f, "SATA", "", "Gen %d (%.1f Gbps)", d->ata.sata_gen, d->ata.sata_gen * 1.5);
            fputs("<div class=\"row\"><span class=\"lbl\">Features</span><span class=\"val\">", f);
            fprintf(f, "<span class=\"pill %s\">NCQ %u</span>", d->ata.ncq_supported ? "p-on" : "p-off", d->ata.queue_depth);
            fprintf(f, "<span class=\"pill %s\">TRIM</span>", d->ata.trim_supported ? "p-on" : "p-off");
            fprintf(f, "<span class=\"pill %s\">48-bit LBA</span>", d->ata.lba48_supported ? "p-on" : "p-off");
            fprintf(f, "<span class=\"pill %s\">Write Cache</span>", d->ata.write_cache ? "p-on" : "p-off");
            fprintf(f, "<span class=\"pill %s\">SMART</span>", d->ata.smart_enabled ? "p-on" : "p-off");
            fputs("</span></div>\n", f);
            sec_close(f);
        }

        if (d->geometry.valid) {
            sec_open(f, "Disk Geometry");
            row(f, "Size", "", "%.2f GB", (double)d->geometry.total_bytes / 1e9);
            row(f, "Bytes/Sector", "", "%lu", (unsigned long)d->geometry.bytes_per_sector);
            sec_close(f);
        }

        if (d->smart.valid) {
            sec_open(f, "SMART Health");
            fprintf(f, "<div class=\"row\"><span class=\"lbl\">Status</span><span class=\"val %s\">%s</span></div>\n",
                    d->smart.health_ok ? "vol" : "warn", d->smart.health_ok ? "PASSED" : "FAILED");
            if (d->smart.has_temp) row(f, "Temperature", "", "%u C", d->smart.temperature);
            if (d->smart.has_poh) row(f, "Power-On Hours", "", "%lu", (unsigned long)d->smart.power_on_hours);
            if (d->smart.has_power_cycles) row(f, "Power Cycles", "", "%lu", (unsigned long)d->smart.power_cycles);
            if (d->smart.has_reallocated) {
                fprintf(f, "<div class=\"row\"><span class=\"lbl\">Realloc Secs</span><span class=\"val %s\">%lu</span></div>\n",
                        d->smart.reallocated_sectors > 0 ? "warn" : "vol", (unsigned long)d->smart.reallocated_sectors);
            }
            if (d->smart.attr_count > 0) {
                fputs("<details style=\"margin-top:6px\"><summary style=\"cursor:pointer;color:#8b949e;font-size:.75rem\">All Attributes</summary>\n", f);
                fputs("<div class=\"smart-grid\" style=\"margin-top:6px\">\n", f);
                fputs("<span class=\"sh\">ID</span><span class=\"sh\">Name</span><span class=\"sh\">Cur</span><span class=\"sh\">Wst</span><span class=\"sh\">Raw</span>\n", f);
                for (int j = 0; j < d->smart.attr_count; j++) {
                    SmartAttr* a = &d->smart.attrs[j];
                    fprintf(f, "<span>0x%02X</span><span>%s</span><span>%u</span><span>%u</span><span>%llu</span>\n",
                            a->id, a->name, a->current, a->worst, (unsigned long long)a->raw);
                }
                fputs("</div></details>\n", f);
            }
            sec_close(f);
        }

        if (d->ids.valid && d->ids.count > 0) {
            sec_open(f, "VPD Page 83 Identifiers");
            for (int j = 0; j < d->ids.count; j++) {
                VpdIdentifier* e = &d->ids.entries[j];
                fprintf(f, "<div class=\"id-e\"><strong>%s</strong> (%s) &mdash; %s</div>\n",
                        e->type_name, e->codeset_name, e->is_ascii ? e->data_ascii : e->data_hex);
            }
            sec_close(f);
        }

        if (d->layout.valid) {
            sec_open(f, "Disk Unique ID");
            if (d->layout.style == 0) {
                row(f, "Style", "", "MBR");
                fprintf(f, "<div class=\"row\"><span class=\"lbl\">MBR Signature</span><span class=\"val hl\">0x%08lX</span></div>\n", (unsigned long)d->layout.mbr_signature);
            } else if (d->layout.style == 1) {
                row(f, "Style", "", "GPT");
                fprintf(f, "<div class=\"row\"><span class=\"lbl\">GPT GUID</span><span class=\"val hl\">%s</span></div>\n", d->layout.gpt_guid);
            }
            if (d->layout.detail_count > 0) {
                fputs("<details style=\"margin-top:4px\"><summary style=\"cursor:pointer;color:#8b949e;font-size:.75rem\">Partitions</summary>\n", f);
                for (int p = 0; p < d->layout.detail_count; p++) {
                    PartitionInfo* pi = &d->layout.parts[p];
                    fprintf(f, "<div class=\"part-row\">Part %u: %.1f GB", pi->number, (double)pi->length / 1e9);
                    if (pi->is_gpt && pi->gpt_name[0]) fprintf(f, " &mdash; \"%s\"", pi->gpt_name);
                    fputs("</div>\n", f);
                }
                fputs("</details>\n", f);
            }
            sec_close(f);
        }

        if (d->mbr.valid) {
            sec_open(f, "Raw MBR");
            row(f, "Disk Sig [0x1B8]", "", "0x%08X", d->mbr.disk_sig);
            fprintf(f, "<div class=\"row\"><span class=\"lbl\">Boot Sig [0x1FE]</span><span class=\"val\">0x%04X %s</span></div>\n",
                    d->mbr.boot_sig, d->mbr.boot_sig == 0xAA55 ? "(valid)" : "(INVALID)");
            sec_close(f);
        }

        if (d->nvme_health.valid) {
            int tc = (int)d->nvme_health.temperature - 273;
            sec_open(f, "NVMe Health Log");
            fprintf(f, "<div class=\"row\"><span class=\"lbl\">Critical Warn</span><span class=\"val %s\">0x%02X</span></div>\n",
                    d->nvme_health.critical_warning ? "warn" : "vol", d->nvme_health.critical_warning);
            row(f, "Temperature", "", "%d C", tc);
            fprintf(f, "<div class=\"row\"><span class=\"lbl\">Avail Spare</span><span class=\"val %s\">%u%%</span></div>\n",
                    d->nvme_health.avail_spare <= d->nvme_health.avail_spare_thresh ? "warn" : "vol", d->nvme_health.avail_spare);
            row(f, "Percent Used", "", "%u%%", d->nvme_health.percent_used);
            row(f, "Data Read", "", "%.2f TB", (double)d->nvme_health.data_units_read * 512000.0 / 1e12);
            row(f, "Data Written", "", "%.2f TB", (double)d->nvme_health.data_units_written * 512000.0 / 1e12);
            row(f, "Power-On Hrs", "", "%llu", (unsigned long long)d->nvme_health.power_on_hours);
            row(f, "Unsafe Shutdn", "", "%llu", (unsigned long long)d->nvme_health.unsafe_shutdowns);
            fprintf(f, "<div class=\"row\"><span class=\"lbl\">Media Errors</span><span class=\"val %s\">%llu</span></div>\n",
                    d->nvme_health.media_errors > 0 ? "warn" : "vol", (unsigned long long)d->nvme_health.media_errors);
            sec_close(f);
        }

        if (d->cache.valid) {
            sec_open(f, "Cache Policy");
            fputs("<div class=\"row\"><span class=\"lbl\">Caching</span><span class=\"val\">", f);
            fprintf(f, "<span class=\"pill %s\">Read Cache</span>", d->cache.read_cache ? "p-on" : "p-off");
            fprintf(f, "<span class=\"pill %s\">Write Cache</span>", d->cache.write_cache ? "p-on" : "p-off");
            fprintf(f, "<span class=\"pill %s\">Power Protect</span>", d->cache.power_protected ? "p-on" : "p-off");
            fputs("</span></div>\n", f);
            row(f, "Write Mode", "", "%s", d->cache.write_through ? "WriteThrough" : "WriteBack");
            sec_close(f);
        }

        if (d->extra.valid) {
            sec_open(f, "Device Properties");
            fputs("<div class=\"row\"><span class=\"lbl\">Detect</span><span class=\"val\">", f);
            fprintf(f, "<span class=\"pill %s\">%s</span>", d->extra.has_seek_penalty ? "p-off" : "p-on",
                    d->extra.has_seek_penalty ? "HDD (seek penalty)" : "SSD (no seek penalty)");
            fprintf(f, "<span class=\"pill %s\">TRIM</span>", d->extra.trim_enabled ? "p-on" : "p-off");
            fputs("</span></div>\n", f);
            if (d->extra.align_valid)
                row(f, "Align Offset", "", "%lu bytes", (unsigned long)d->extra.align_byte_offset);
            sec_close(f);
        }

        if (d->perf.valid) {
            sec_open(f, "Performance Counters");
            row(f, "Reads", "", "%llu (%.2f GB)", (unsigned long long)d->perf.read_count, (double)d->perf.bytes_read / 1e9);
            row(f, "Writes", "", "%llu (%.2f GB)", (unsigned long long)d->perf.write_count, (double)d->perf.bytes_written / 1e9);
            row(f, "Queue Depth", "", "%lu", (unsigned long)d->perf.queue_depth);
            sec_close(f);
        }

        if (d->smart_thresh.valid && d->smart_thresh.count > 0) {
            sec_open(f, "SMART Thresholds");
            for (int j = 0; j < d->smart_thresh.count; j++) {
                SmartThreshold* t = &d->smart_thresh.entries[j];
                fprintf(f, "<div class=\"row\"><span class=\"lbl\">ID %u</span><span class=\"val%s\">Thresh=%u %s</span></div>\n",
                        t->id, t->exceeded ? " warn" : "", t->threshold, t->exceeded ? "EXCEEDED" : "OK");
            }
            sec_close(f);
        }

        if (d->nvme_id.valid) {
            sec_open(f, "NVMe Identify Controller");
            row(f, "Serial", "", "%s", d->nvme_id.serial);
            row(f, "Model", "", "%s", d->nvme_id.model);
            row(f, "Firmware", "", "%s", d->nvme_id.firmware);
            row(f, "PCI VID", "", "0x%04X", d->nvme_id.vid);
            row(f, "Subsys VID", "", "0x%04X", d->nvme_id.ssvid);
            row(f, "NVMe Version", "", "%u.%u.%u", (d->nvme_id.ver>>16)&0xFF, (d->nvme_id.ver>>8)&0xFF, d->nvme_id.ver&0xFF);
            row(f, "IEEE OUI", "", "%02X:%02X:%02X", d->nvme_id.ieee_oui[0], d->nvme_id.ieee_oui[1], d->nvme_id.ieee_oui[2]);
            row(f, "Namespaces", "", "%u", d->nvme_id.num_namespaces);
            row(f, "Total NVM", "", "%.2f GB", (double)d->nvme_id.total_cap_bytes / 1e9);
            sec_close(f);
        }

        if (d->nvme_fw.valid) {
            sec_open(f, "NVMe Firmware Slots");
            row(f, "Active Slot", "", "%u", d->nvme_fw.active_slot);
            for (int s = 0; s < MAX_FW_SLOTS; s++) {
                if (d->nvme_fw.slot_rev[s][0])
                    row(f, "Slot", "", "%d: %s%s", s+1, d->nvme_fw.slot_rev[s],
                        (s+1 == d->nvme_fw.active_slot) ? " [ACTIVE]" : "");
            }
            sec_close(f);
        }

        if (d->ata_sec.valid && d->ata_sec.supported) {
            sec_open(f, "ATA Security");
            row(f, "Enabled", "", "%s", d->ata_sec.enabled ? "Yes" : "No");
            row(f, "Locked", d->ata_sec.locked ? "warn" : "", "%s", d->ata_sec.locked ? "YES" : "No");
            row(f, "Frozen", "", "%s", d->ata_sec.frozen ? "Yes" : "No");
            row(f, "Enh. Erase", "", "%s", d->ata_sec.enhanced_erase ? "Yes" : "No");
            sec_close(f);
        }

        if (d->power.valid) {
            sec_open(f, "Power Mode");
            row(f, "State", "", "%s (0x%02X)", d->power.mode_name, d->power.mode);
            sec_close(f);
        }

        if (d->hpa.valid) {
            sec_open(f, "Host Protected Area");
            row(f, "Native Max", "", "%llu LBA", (unsigned long long)d->hpa.native_max_lba);
            row(f, "Current Max", "", "%llu LBA", (unsigned long long)d->hpa.current_max_lba);
            row(f, "HPA Active", d->hpa.hpa_active ? "warn" : "", "%s", d->hpa.hpa_active ? "YES" : "No");
            if (d->hpa.hpa_active) row(f, "Hidden", "warn", "%llu sectors", (unsigned long long)d->hpa.hidden_sectors);
            sec_close(f);
        }

        if (d->dco.valid) {
            sec_open(f, "Device Config Overlay");
            row(f, "Real Max LBA", "", "%llu", (unsigned long long)d->dco.real_max_lba);
            row(f, "DCO Active", d->dco.dco_active ? "warn" : "", "%s", d->dco.dco_active ? "YES" : "No");
            row(f, "Features Hidden", "", "%s", d->dco.features_disabled ? "Yes" : "No");
            sec_close(f);
        }

        if (d->sed.valid) {
            sec_open(f, "Self-Encrypting Drive");
            row(f, "SED Capable", "", "%s", d->sed.sed_capable ? "Yes" : "No");
            if (d->sed.desc[0]) row(f, "Standard", "", "%s", d->sed.desc);
            row(f, "Locked", d->sed.locked ? "warn" : "", "%s", d->sed.locked ? "YES" : "No");
            sec_close(f);
        }

        if (d->scsi_modes.valid) {
            sec_open(f, "SCSI Mode Pages");
            row(f, "Write Cache", "", "%s", d->scsi_modes.write_cache ? "Enabled" : "Disabled");
            row(f, "Read Cache", "", "%s", d->scsi_modes.read_cache ? "Enabled" : "Disabled");
            row(f, "Auto Write Realloc", "", "%s", d->scsi_modes.awre ? "Yes" : "No");
            row(f, "Auto Read Realloc", "", "%s", d->scsi_modes.arre ? "Yes" : "No");
            row(f, "Read Retry Count", "", "%u", d->scsi_modes.error_recovery);
            sec_close(f);
        }

        if (d->devpath.valid) {
            sec_open(f, "Device Path");
            if (d->devpath.friendly_name[0]) row(f, "Name", "", "%s", d->devpath.friendly_name);
            if (d->devpath.device_path[0]) row(f, "Instance", "", "%s", d->devpath.device_path);
            if (d->devpath.hw_id[0]) row(f, "Hardware ID", "", "%s", d->devpath.hw_id);
            if (d->devpath.location[0]) row(f, "Location", "", "%s", d->devpath.location);
            sec_close(f);
        }

        if (d->volumes.count > 0) {
            sec_open(f, "Volumes");
            for (int j = 0; j < d->volumes.count; j++) {
                VolumeInfo* v = &d->volumes.vols[j];
                if (v->letter) {
                    fprintf(f, "<div class=\"row\"><span class=\"lbl\">%c:\\</span>"
                               "<span class=\"val vol\">0x%08lX</span>"
                               "<span class=\"val\" style=\"margin-left:12px\">%s &mdash; \"%s\"</span></div>\n",
                            v->letter, (unsigned long)v->serial, v->fs_name, v->label);
                } else if (v->mount_point[0]) {
                    fprintf(f, "<div class=\"row\"><span class=\"lbl\">%s</span>"
                               "<span class=\"val\">%s</span></div>\n",
                            v->mount_point, v->fs_name);
                }
            }
            sec_close(f);
        }

        fputs("</details>\n", f);
    }

    fprintf(f, "</div>\n<div class=\"footer\">Generated by disk-layout-scanner %s</div>\n</body></html>\n",
            DISK_LAYOUT_SCANNER_VERSION);
    int io_err = ferror(f);
    if (fclose(f) != 0) io_err = 1;
    if (io_err) {
        fprintf(stderr, "disk-layout-scanner: error writing %s\n", filename);
        return -1;
    }
    return 0;
}
