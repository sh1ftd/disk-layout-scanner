#include "../core/disk_info.h"

static void jstr(FILE* fp, const char* s) {
    fputc('"', fp);
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"') fputs("\\\"", fp);
        else if (c == '\\') fputs("\\\\", fp);
        else if (c == '\n') fputs("\\n", fp);
        else if (c == '\r') fputs("\\r", fp);
        else if (c == '\t') fputs("\\t", fp);
        else if (c < 0x20) fprintf(fp, "\\u%04x", c);
        else fputc(c, fp);
    }
    fputc('"', fp);
}

void output_json(DiskInfo* disks, int count, FILE* fp) {
    fprintf(fp, "[\n");
    for (int i = 0; i < count; i++) {
        DiskInfo* d = &disks[i];
        fprintf(fp, "  {\n    \"drive_number\": %d,\n", d->drive_number);

        fprintf(fp, "    \"storage\": {\"valid\":%d", d->storage.valid);
        if (d->storage.valid) {
            fprintf(fp, ",\"vendor\":"); jstr(fp, d->storage.vendor);
            fprintf(fp, ",\"product\":"); jstr(fp, d->storage.product);
            fprintf(fp, ",\"revision\":"); jstr(fp, d->storage.revision);
            fprintf(fp, ",\"serial\":"); jstr(fp, d->storage.serial);
            fprintf(fp, ",\"bus_type\":%d,\"bus_name\":", d->storage.bus_type); jstr(fp, d->storage.bus_name);
            fprintf(fp, ",\"removable\":%d", d->storage.removable);
        }
        fprintf(fp, "},\n");

        fprintf(fp, "    \"ata\": {\"valid\":%d", d->ata.valid);
        if (d->ata.valid) {
            fprintf(fp, ",\"serial\":"); jstr(fp, d->ata.serial);
            fprintf(fp, ",\"firmware\":"); jstr(fp, d->ata.firmware);
            fprintf(fp, ",\"model\":"); jstr(fp, d->ata.model);
            fprintf(fp, ",\"lba28\":%u,\"lba48\":%llu", d->ata.lba28, (unsigned long long)d->ata.lba48);
            fprintf(fp, ",\"capacity_gb\":%.1f", d->ata.capacity_gb);
            fprintf(fp, ",\"wwn\":\"%016llX\"", (unsigned long long)d->ata.wwn);
            fprintf(fp, ",\"naa\":%u,\"oui\":\"%06X\"", d->ata.naa, d->ata.oui);
            fprintf(fp, ",\"is_ssd\":%d,\"rpm\":%u,\"cache_kb\":%u", d->ata.is_ssd, d->ata.rpm, d->ata.cache_kb);
            fprintf(fp, ",\"queue_depth\":%u,\"sata_gen\":%d,\"ncq\":%d", d->ata.queue_depth, d->ata.sata_gen, d->ata.ncq_supported);
            fprintf(fp, ",\"trim\":%d,\"lba48_support\":%d,\"write_cache\":%d", d->ata.trim_supported, d->ata.lba48_supported, d->ata.write_cache);
            fprintf(fp, ",\"smart_supported\":%d,\"smart_enabled\":%d", d->ata.smart_supported, d->ata.smart_enabled);
            fprintf(fp, ",\"log_sector_size\":%u,\"phys_log_ratio\":%u", d->ata.log_sector_size, d->ata.phys_log_ratio);
            fprintf(fp, ",\"form_factor\":%u,\"form_factor_name\":", d->ata.form_factor); jstr(fp, form_factor_name(d->ata.form_factor));
            fprintf(fp, ",\"ata_version\":"); jstr(fp, ata_version_name(d->ata.ata_major));
            fprintf(fp, ",\"udma_mode\":%u", d->ata.udma_mode);
        }
        fprintf(fp, "},\n");

        fprintf(fp, "    \"geometry\": {\"valid\":%d", d->geometry.valid);
        if (d->geometry.valid) {
            fprintf(fp, ",\"total_bytes\":%llu", (unsigned long long)d->geometry.total_bytes);
            fprintf(fp, ",\"bytes_per_sector\":%lu", (unsigned long)d->geometry.bytes_per_sector);
            fprintf(fp, ",\"cylinders\":%llu,\"heads\":%lu,\"spt\":%lu",
                    (unsigned long long)d->geometry.cylinders, (unsigned long)d->geometry.heads, (unsigned long)d->geometry.sectors_per_track);
        }
        fprintf(fp, "},\n");

        fprintf(fp, "    \"smart\": {\"valid\":%d", d->smart.valid);
        if (d->smart.valid) {
            fprintf(fp, ",\"health_ok\":%d", d->smart.health_ok);
            fprintf(fp, ",\"temperature\":%u,\"power_on_hours\":%lu,\"power_cycles\":%lu,\"reallocated\":%lu",
                    d->smart.temperature, (unsigned long)d->smart.power_on_hours,
                    (unsigned long)d->smart.power_cycles, (unsigned long)d->smart.reallocated_sectors);
            fprintf(fp, ",\"attrs\":[");
            for (int j = 0; j < d->smart.attr_count; j++) {
                SmartAttr* a = &d->smart.attrs[j];
                if (j) fputc(',', fp);
                fprintf(fp, "{\"id\":%u,\"name\":", a->id); jstr(fp, a->name);
                fprintf(fp, ",\"current\":%u,\"worst\":%u,\"raw\":%llu}", a->current, a->worst, (unsigned long long)a->raw);
            }
            fprintf(fp, "]");
        }
        fprintf(fp, "},\n");

        fprintf(fp, "    \"vpd_ids\": {\"valid\":%d,\"entries\":[", d->ids.valid);
        for (int j = 0; j < d->ids.count; j++) {
            VpdIdentifier* e = &d->ids.entries[j];
            if (j) fputc(',', fp);
            fprintf(fp, "{\"type\":%d,\"type_name\":", e->type); jstr(fp, e->type_name);
            fprintf(fp, ",\"data\":"); jstr(fp, e->is_ascii ? e->data_ascii : e->data_hex);
            fprintf(fp, ",\"data_hex\":"); jstr(fp, e->data_hex);
            fputc('}', fp);
        }
        fprintf(fp, "]},\n");

        fprintf(fp, "    \"layout\": {\"valid\":%d", d->layout.valid);
        if (d->layout.valid) {
            fprintf(fp, ",\"style\":%d,\"mbr_sig\":\"0x%08lX\"", d->layout.style, (unsigned long)d->layout.mbr_signature);
            fprintf(fp, ",\"gpt_guid\":"); jstr(fp, d->layout.gpt_guid);
            fprintf(fp, ",\"partitions\":%lu,\"details\":[", (unsigned long)d->layout.partition_count);
            for (int p = 0; p < d->layout.detail_count; p++) {
                PartitionInfo* pi = &d->layout.parts[p];
                if (p) fputc(',', fp);
                fprintf(fp, "{\"number\":%u,\"offset\":%llu,\"length\":%llu,\"is_gpt\":%d",
                        pi->number, (unsigned long long)pi->offset, (unsigned long long)pi->length, pi->is_gpt);
                if (pi->is_gpt) { fprintf(fp, ",\"type_guid\":"); jstr(fp, pi->gpt_type_guid); fprintf(fp, ",\"name\":"); jstr(fp, pi->gpt_name); }
                else fprintf(fp, ",\"mbr_type\":%u", pi->mbr_type);
                fputc('}', fp);
            }
            fputc(']', fp);
        }
        fprintf(fp, "},\n");

        fprintf(fp, "    \"raw_mbr\": {\"valid\":%d", d->mbr.valid);
        if (d->mbr.valid)
            fprintf(fp, ",\"disk_sig\":\"0x%08X\",\"boot_sig\":\"0x%04X\",\"boot_valid\":%d,\"part1_type\":\"0x%02X\"",
                    d->mbr.disk_sig, d->mbr.boot_sig, d->mbr.boot_sig == 0xAA55, d->mbr.part1_type);
        fprintf(fp, "},\n");

        fprintf(fp, "    \"nvme_health\": {\"valid\":%d", d->nvme_health.valid);
        if (d->nvme_health.valid) {
            fprintf(fp, ",\"critical_warning\":%u,\"temperature_k\":%u,\"avail_spare\":%u,\"percent_used\":%u",
                    d->nvme_health.critical_warning, d->nvme_health.temperature, d->nvme_health.avail_spare, d->nvme_health.percent_used);
            fprintf(fp, ",\"data_units_read\":%llu,\"data_units_written\":%llu",
                    (unsigned long long)d->nvme_health.data_units_read, (unsigned long long)d->nvme_health.data_units_written);
            fprintf(fp, ",\"power_on_hours\":%llu,\"unsafe_shutdowns\":%llu,\"media_errors\":%llu",
                    (unsigned long long)d->nvme_health.power_on_hours, (unsigned long long)d->nvme_health.unsafe_shutdowns,
                    (unsigned long long)d->nvme_health.media_errors);
        }
        fprintf(fp, "},\n");

        fprintf(fp, "    \"cache\": {\"valid\":%d", d->cache.valid);
        if (d->cache.valid)
            fprintf(fp, ",\"read_cache\":%d,\"write_cache\":%d,\"write_through\":%d,\"power_protected\":%d",
                    d->cache.read_cache, d->cache.write_cache, d->cache.write_through, d->cache.power_protected);
        fprintf(fp, "},\n");

        fprintf(fp, "    \"extra\": {\"valid\":%d", d->extra.valid);
        if (d->extra.valid)
            fprintf(fp, ",\"seek_penalty\":%d,\"trim_enabled\":%d,\"align_offset\":%lu",
                    d->extra.has_seek_penalty, d->extra.trim_enabled, (unsigned long)d->extra.align_byte_offset);
        fprintf(fp, "},\n");

        fprintf(fp, "    \"performance\": {\"valid\":%d", d->perf.valid);
        if (d->perf.valid)
            fprintf(fp, ",\"bytes_read\":%llu,\"bytes_written\":%llu,\"read_count\":%llu,\"write_count\":%llu,\"queue_depth\":%lu",
                    (unsigned long long)d->perf.bytes_read, (unsigned long long)d->perf.bytes_written,
                    (unsigned long long)d->perf.read_count, (unsigned long long)d->perf.write_count, (unsigned long)d->perf.queue_depth);
        fprintf(fp, "},\n");

        fprintf(fp, "    \"smart_thresholds\": {\"valid\":%d", d->smart_thresh.valid);
        if (d->smart_thresh.valid && d->smart_thresh.count > 0) {
            fprintf(fp, ",\"entries\":[");
            for (int j = 0; j < d->smart_thresh.count; j++) {
                if (j) fputc(',', fp);
                fprintf(fp, "{\"id\":%u,\"threshold\":%u,\"exceeded\":%d}",
                        d->smart_thresh.entries[j].id, d->smart_thresh.entries[j].threshold, d->smart_thresh.entries[j].exceeded);
            }
            fprintf(fp, "]");
        }
        fprintf(fp, "},\n");

        fprintf(fp, "    \"nvme_identify\": {\"valid\":%d", d->nvme_id.valid);
        if (d->nvme_id.valid) {
            fprintf(fp, ",\"serial\":"); jstr(fp, d->nvme_id.serial);
            fprintf(fp, ",\"model\":"); jstr(fp, d->nvme_id.model);
            fprintf(fp, ",\"firmware\":"); jstr(fp, d->nvme_id.firmware);
            fprintf(fp, ",\"vid\":\"0x%04X\",\"ssvid\":\"0x%04X\",\"ctrl_id\":%u", d->nvme_id.vid, d->nvme_id.ssvid, d->nvme_id.ctrl_id);
            fprintf(fp, ",\"version\":\"%u.%u.%u\"", (d->nvme_id.ver>>16)&0xFF, (d->nvme_id.ver>>8)&0xFF, d->nvme_id.ver&0xFF);
            fprintf(fp, ",\"namespaces\":%u,\"total_cap\":%llu,\"mdts\":%u",
                    d->nvme_id.num_namespaces, (unsigned long long)d->nvme_id.total_cap_bytes, d->nvme_id.max_transfer_sz);
        }
        fprintf(fp, "},\n");

        fprintf(fp, "    \"nvme_fw_slots\": {\"valid\":%d", d->nvme_fw.valid);
        if (d->nvme_fw.valid) {
            fprintf(fp, ",\"active\":%u,\"pending\":%u,\"slots\":[", d->nvme_fw.active_slot, d->nvme_fw.pending_slot);
            for (int s = 0; s < MAX_FW_SLOTS; s++) {
                if (s) fputc(',', fp);
                jstr(fp, d->nvme_fw.slot_rev[s]);
            }
            fprintf(fp, "]");
        }
        fprintf(fp, "},\n");

        fprintf(fp, "    \"ata_security\": {\"valid\":%d", d->ata_sec.valid);
        if (d->ata_sec.valid)
            fprintf(fp, ",\"supported\":%d,\"enabled\":%d,\"locked\":%d,\"frozen\":%d,\"enhanced_erase\":%d,\"master_pwd_cap\":%d",
                    d->ata_sec.supported, d->ata_sec.enabled, d->ata_sec.locked, d->ata_sec.frozen,
                    d->ata_sec.enhanced_erase, d->ata_sec.master_pwd_cap);
        fprintf(fp, "},\n");

        fprintf(fp, "    \"power_mode\": {\"valid\":%d", d->power.valid);
        if (d->power.valid) {
            fprintf(fp, ",\"mode\":\"0x%02X\",\"name\":", d->power.mode); jstr(fp, d->power.mode_name);
        }
        fprintf(fp, "},\n");

        fprintf(fp, "    \"hpa\": {\"valid\":%d", d->hpa.valid);
        if (d->hpa.valid)
            fprintf(fp, ",\"native_max\":%llu,\"current_max\":%llu,\"active\":%d,\"hidden\":%llu",
                    (unsigned long long)d->hpa.native_max_lba, (unsigned long long)d->hpa.current_max_lba,
                    d->hpa.hpa_active, (unsigned long long)d->hpa.hidden_sectors);
        fprintf(fp, "},\n");

        fprintf(fp, "    \"dco\": {\"valid\":%d", d->dco.valid);
        if (d->dco.valid)
            fprintf(fp, ",\"real_max\":%llu,\"active\":%d,\"features_disabled\":%d",
                    (unsigned long long)d->dco.real_max_lba, d->dco.dco_active, d->dco.features_disabled);
        fprintf(fp, "},\n");

        fprintf(fp, "    \"sed_opal\": {\"valid\":%d", d->sed.valid);
        if (d->sed.valid) {
            fprintf(fp, ",\"sed_capable\":%d,\"opal_v1\":%d,\"opal_v2\":%d,\"enterprise\":%d,\"locked\":%d",
                    d->sed.sed_capable, d->sed.opal_v1, d->sed.opal_v2, d->sed.enterprise_ssc, d->sed.locked);
            fprintf(fp, ",\"desc\":"); jstr(fp, d->sed.desc);
        }
        fprintf(fp, "},\n");

        fprintf(fp, "    \"scsi_modes\": {\"valid\":%d", d->scsi_modes.valid);
        if (d->scsi_modes.valid)
            fprintf(fp, ",\"write_cache\":%d,\"read_cache\":%d,\"awre\":%d,\"arre\":%d,\"read_retry\":%u",
                    d->scsi_modes.write_cache, d->scsi_modes.read_cache, d->scsi_modes.awre,
                    d->scsi_modes.arre, d->scsi_modes.error_recovery);
        fprintf(fp, "},\n");

        fprintf(fp, "    \"devpath\": {\"valid\":%d", d->devpath.valid);
        if (d->devpath.valid) {
            fprintf(fp, ",\"instance\":"); jstr(fp, d->devpath.device_path);
            fprintf(fp, ",\"friendly\":"); jstr(fp, d->devpath.friendly_name);
            fprintf(fp, ",\"hw_id\":"); jstr(fp, d->devpath.hw_id);
            fprintf(fp, ",\"location\":"); jstr(fp, d->devpath.location);
        }
        fprintf(fp, "},\n");

        fprintf(fp, "    \"volumes\": [");
        for (int j = 0; j < d->volumes.count; j++) {
            VolumeInfo* v = &d->volumes.vols[j];
            if (j) fputc(',', fp);
            if (v->letter)
                fprintf(fp, "{\"letter\":\"%c\",\"serial\":\"0x%08lX\",\"fs\":", v->letter, (unsigned long)v->serial);
            else
                fprintf(fp, "{\"letter\":\"\",\"serial\":\"0x%08lX\",\"fs\":", (unsigned long)v->serial);
            jstr(fp, v->fs_name); fprintf(fp, ",\"label\":"); jstr(fp, v->label);
            fprintf(fp, ",\"guid\":"); jstr(fp, v->guid_path);
            fputc('}', fp);
        }
        fprintf(fp, "]\n  }%s\n", i < count-1 ? "," : "");
    }
    fprintf(fp, "]\n");
}
