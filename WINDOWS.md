# Windows — `DiskInfo` extraction reference

<p align="center"><a href="README.md"><b>↑ Back to README</b></a></p>

Per-disk handle: `\\.\PhysicalDriveN` for `N` in `0 .. MAX_DRIVES-1`. Implementation: `queries_win.c` → `scan_disks`, `disk_info.h`.

Unless noted, fields are written only when the underlying `DeviceIoControl` / `ReadFile` / SetupAPI call succeeds.

---

## Top-level (`DiskInfo`)

| Member                       | Source                                 |
| ---------------------------- | -------------------------------------- |
| `drive_number`               | Loop index `N` for `PhysicalDriveN`    |
| `dev_path`                   | `sprintf("\\\\.\\PhysicalDrive%d", N)` |
| _(all nested structs below)_ | See sections                           |

---

## `StoragePropInfo` (`storage`)

`IOCTL_STORAGE_QUERY_PROPERTY`, `PropertyId = StorageDeviceProperty`, `PropertyStandardQuery`.

| Member      | Source                                         |
| ----------- | ---------------------------------------------- |
| `valid`     | Descriptor returned                            |
| `vendor`    | `STORAGE_DEVICE_DESCRIPTOR` → `VendorIdOffset` |
| `product`   | `ProductIdOffset`                              |
| `revision`  | `ProductRevisionOffset`                        |
| `serial`    | `SerialNumberOffset`                           |
| `bus_type`  | `BusType`                                      |
| `bus_name`  | `bus_type_name(BusType)`                       |
| `removable` | `RemovableMedia`                               |

---

## `AtaIdentifyInfo` (`ata`)

`IOCTL_ATA_PASS_THROUGH` / `IOCTL_ATA_PASS_THROUGH_DIRECT`, command **IDENTIFY DEVICE** (`0xEC`), 512-byte IDENTIFY buffer → `query_ata_id`.

| Member                                                               | IDENTIFY decode (words / bits) |
| -------------------------------------------------------------------- | ------------------------------ |
| `valid`                                                              | IOCTL success                  |
| `serial`                                                             | Words 10–19 (ATA string swap)  |
| `firmware`                                                           | 23–26                          |
| `model`                                                              | 27–46                          |
| `lba28`                                                              | 60–61                          |
| `lba48`                                                              | 100–103                        |
| `capacity_gb`                                                        | From LBA48 or LBA28 × 512      |
| `wwn`, `naa`, `oui`, `vendor_specific`                               | 108–111 (NAA-5 / NAA-6 decode) |
| `rpm` / `is_ssd`                                                     | Word 217                       |
| `cache_kb`                                                           | Word 21                        |
| `queue_depth`                                                        | Word 75 (low 5 bits + 1)       |
| `sata_gen`, `ncq_supported`                                          | Word 76                        |
| `smart_supported`, `lba48_supported`, `smart_enabled`, `write_cache` | Words 82, 83, 85               |
| `trim_supported`                                                     | Word 169 bit 0                 |
| `log_sector_size`, `phys_log_ratio`                                  | Word 106 (+ 117–118 if 4Kn)    |
| `form_factor`                                                        | Word 168 low nibble            |
| `ata_major`                                                          | Word 80                        |
| `udma_mode`                                                          | Word 88 active UDMA            |
| `transport_major`                                                    | Word 222                       |

---

## `DeviceIdsInfo` (`ids`)

`IOCTL_STORAGE_QUERY_PROPERTY`, `PropertyId = StorageDeviceIdProperty`, walks `STORAGE_IDENTIFIER` list (max `MAX_IDENTIFIERS`).

| Member                                | Source                                      |
| ------------------------------------- | ------------------------------------------- |
| `valid`, `count`                      | At least one identifier                     |
| `entries[i].type`                     | `STORAGE_IDENTIFIER.Type`                   |
| `type_name`                           | `id_type_name`                              |
| `code_set`, `codeset_name`            | `CodeSet` + `codeset_name`                  |
| `size`                                | `IdentifierSize`                            |
| `data_hex` / `data_ascii`, `is_ascii` | Payload; ASCII path if code set ASCII/UTF-8 |

---

## `GeometryInfo` (`geometry`)

`IOCTL_DISK_GET_DRIVE_GEOMETRY_EX` → `DISK_GEOMETRY_EX`.

| Member              | Source                       |
| ------------------- | ---------------------------- |
| `valid`             | IOCTL success                |
| `total_bytes`       | `DiskSize`                   |
| `bytes_per_sector`  | `Geometry.BytesPerSector`    |
| `media_type`        | `Geometry.MediaType`         |
| `cylinders`         | `Geometry.Cylinders`         |
| `heads`             | `Geometry.TracksPerCylinder` |
| `sectors_per_track` | `Geometry.SectorsPerTrack`   |

---

## `SmartInfo` (`smart`)

`SMART_SEND_DRIVE_COMMAND` (enable ops, return status — **return status not decoded into a field**); `SMART_RCV_DRIVE_DATA` READ VALUES (`0xD0` path in code).

| Member                                   | Source                                                                           |
| ---------------------------------------- | -------------------------------------------------------------------------------- |
| `valid`                                  | RCV success                                                                      |
| `health_ok`                              | Default **1** after READ VALUES (not updated from RETURN STATUS in current code) |
| `attrs[]`                                | 30×12-byte vendor attribute slots; `id`, `flags`, `current`, `worst`, `raw`      |
| `attr_count`                             | Non-zero IDs                                                                     |
| `has_temp`, `temperature`                | Attr **0xC2** / **0xBE**                                                         |
| `has_poh`, `power_on_hours`              | **0x09**                                                                         |
| `has_power_cycles`, `power_cycles`       | **0x0C**                                                                         |
| `has_reallocated`, `reallocated_sectors` | **0x05**                                                                         |

---

## `SmartThreshInfo` (`smart_thresh`)

`SMART_RCV_DRIVE_DATA`, features **`0xD1`** (threshold page).

| Member                      | Source                                   |
| --------------------------- | ---------------------------------------- |
| `valid`, `count`            | RCV success; non-zero threshold IDs      |
| `entries[].id`, `threshold` | Bytes 0–1 of each 12-byte slot           |
| `entries[].exceeded`        | Compared to matching `SmartAttr.current` |

---

## `NvmeHealthInfo` (`nvme_health`)

`IOCTL_STORAGE_QUERY_PROPERTY`, `StorageDeviceProtocolSpecificProperty`, `ProtocolTypeNvme`, `NVMeDataTypeLogPage`, log **0x02**, 512-byte buffer.

| Member                                              | Log offset (bytes) |
| --------------------------------------------------- | ------------------ |
| `valid`                                             | IOCTL success      |
| `critical_warning`                                  | 0                  |
| `temperature`                                       | 1–2 (UINT16 LE)    |
| `avail_spare`, `avail_spare_thresh`, `percent_used` | 3–5                |
| `data_units_read`, `data_units_written`             | 32–39, 48–55       |
| `host_read_cmds`, `host_write_cmds`                 | 64–71, 80–87       |
| `power_on_hours`                                    | 128–135            |
| `unsafe_shutdowns`                                  | 144–151            |
| `media_errors`                                      | 160–167            |
| `error_log_entries`                                 | 176–183            |

---

## `NvmeIdentifyInfo` (`nvme_id`)

Same IOCTL path, Identify controller buffer (4096 B), `query_nvme_identify`.

| Member                                 | Identify layout          |
| -------------------------------------- | ------------------------ |
| `valid`                                | IOCTL success            |
| `serial`                               | Bytes 4–23 (trim spaces) |
| `model`                                | 24–63                    |
| `firmware`                             | 64–71                    |
| `vid`, `ssvid`                         | 0–1, 2–3 LE              |
| `max_transfer_sz`                      | Byte 77                  |
| `ctrl_id`                              | 78–79 LE                 |
| `ver`                                  | 80–83 LE                 |
| `ieee_oui`                             | 73–75                    |
| `num_namespaces`                       | DWORD at 516, low byte   |
| `total_cap_bytes`, `unalloc_cap_bytes` | 280–287, 296–303         |

---

## `NvmeFwSlotInfo` (`nvme_fw`)

NVMe Get Log Page — **Firmware Slot Information** (same protocol IOCTL path).

| Member                        | Source                          |
| ----------------------------- | ------------------------------- |
| `valid`                       | IOCTL success                   |
| `active_slot`, `pending_slot` | Byte 0 (afi) low / high nibbles |
| `slot_rev[s][…]`              | 8-byte ASCII per slot from log  |

---

## `CacheInfo` (`cache`)

`IOCTL_DISK_GET_CACHE_INFORMATION` → `DISK_CACHE_INFORMATION`.

| Member            | Source                                |
| ----------------- | ------------------------------------- |
| `valid`           | IOCTL success                         |
| `read_cache`      | `ReadCacheEnabled`                    |
| `write_cache`     | `WriteCacheEnabled`                   |
| `write_through`   | Derived: `!WriteCacheEnabled` in code |
| `power_protected` | **Never assigned** (remains 0)        |

---

## `ExtraPropsInfo` (`extra`)

`IOCTL_STORAGE_QUERY_PROPERTY` with `PropertyId` **7** (seek penalty), **8** (TRIM), **6** (alignment); `BOOL` / `UINT32` at fixed offsets in returned buffer.

| Member                             | Property ID                                                                                 |
| ---------------------------------- | ------------------------------------------------------------------------------------------- |
| `valid`                            | Always set **1** after `query_extra_props` runs (partial probes may leave TRIM/align unset) |
| `has_seek_penalty`                 | ID **7**, `BOOL` at offset +8                                                               |
| `trim_enabled`                     | ID **8**, `BOOL` at +8                                                                      |
| `align_byte_offset`, `align_valid` | ID **6**, `UINT32` at +12                                                                   |

---

## `PerfInfo` (`perf`)

`IOCTL_DISK_PERFORMANCE` → `DISK_PERFORMANCE`.

| Member                          | Source                           |
| ------------------------------- | -------------------------------- |
| `valid`                         | IOCTL success                    |
| `bytes_read`, `bytes_written`   | `BytesRead`, `BytesWritten`      |
| `read_count`, `write_count`     | `ReadCount`, `WriteCount`        |
| `read_time_ns`, `write_time_ns` | `ReadTime`, `WriteTime` × 100 ns |
| `idle_time_ns`                  | `IdleTime` × 100 ns              |
| `queue_depth`                   | `QueueDepth`                     |

---

## `DriveLayoutInfo` (`layout`) + `PartitionInfo` (`parts[]`)

`IOCTL_DISK_GET_DRIVE_LAYOUT_EX` → `DRIVE_LAYOUT_INFORMATION_EX`.

| Member                               | Source                                                                   |
| ------------------------------------ | ------------------------------------------------------------------------ |
| `valid`                              | IOCTL + partition array sane                                             |
| `style`                              | `0` MBR, `1` GPT, `2` other                                              |
| `mbr_signature`                      | `Mbr.Signature` if MBR                                                   |
| `gpt_guid`                           | `Gpt.DiskId` string if GPT                                               |
| `partition_count`                    | `PartitionCount`                                                         |
| `detail_count`, `parts[]`            | Nonzero-length `PARTITION_INFORMATION_EX` entries (max `MAX_PARTITIONS`) |
| `parts[].offset`, `length`, `number` | `StartingOffset`, `PartitionLength`, `PartitionNumber`                   |
| `parts[].is_gpt`, `mbr_type`         | MBR branch                                                               |
| `parts[].gpt_type_guid`, `gpt_name`  | GPT branch (`WideCharToMultiByte` on name)                               |

---

## `RawMbrInfo` (`mbr`)

`ReadFile` 512 bytes @ offset 0.

| Member       | Byte offset |
| ------------ | ----------- |
| `valid`      | Read 512 OK |
| `disk_sig`   | 0x1B8–0x1BB |
| `boot_sig`   | 0x1FE–0x1FF |
| `part1_type` | 0x1C2       |

---

## `AtaSecurityInfo` (`ata_sec`)

Second IDENTIFY via `IOCTL_ATA_PASS_THROUGH`; word **128**.

| Member           | Word 128 bits |
| ---------------- | ------------- |
| `valid`          | IOCTL success |
| `supported`      | bit 0         |
| `enabled`        | bit 1         |
| `locked`         | bit 2         |
| `frozen`         | bit 3         |
| `count_expired`  | bit 4         |
| `enhanced_erase` | bit 5         |
| `master_pwd_cap` | bit 8         |

---

## `PowerModeInfo` (`power`)

ATA **CHECK POWER MODE** via `IOCTL_ATA_PASS_THROUGH`.

| Member      | Source                                    |
| ----------- | ----------------------------------------- |
| `valid`     | IOCTL success                             |
| `mode`      | CurrentTaskFile[1]                        |
| `mode_name` | Hardcoded map + `Unknown (0x..)` fallback |

---

## `HpaInfo` (`hpa`)

ATA **READ NATIVE MAX ADDRESS** / **READ NATIVE MAX ADDRESS EXT** vs IDENTIFY max LBA.

| Member                         | Rule                                         |
| ------------------------------ | -------------------------------------------- |
| `valid`                        | Native max read OK                           |
| `native_max_lba`               | From taskfile decode (28-bit or 48-bit path) |
| `current_max_lba`              | `lba48` or `lba28` from `AtaIdentifyInfo`    |
| `hpa_active`, `hidden_sectors` | `native_max_lba > current_max_lba`           |

---

## `DcoInfo` (`dco`)

DCO IDENTIFY words after unlock path in `query_dco` (uses `HpaInfo`).

| Member              | Source                              |
| ------------------- | ----------------------------------- |
| `valid`             | IOCTL / parse success               |
| `real_max_lba`      | Words 2–5 of DCO buffer             |
| `dco_active`        | `real_max_lba > hpa.native_max_lba` |
| `features_disabled` | Word 1 not `0xFFFF`/`0x0000`        |

---

## `SedOpalInfo` (`sed`)

Trusted receive buffer (`0x5C` ATA passthrough), feature list walk.

| Member                                                                       | Source                                                                               |
| ---------------------------------------------------------------------------- | ------------------------------------------------------------------------------------ |
| `valid`, `sed_capable`                                                       | Successful parse                                                                     |
| `locked`                                                                     | Feature **0x0002** byte bit 2                                                        |
| `enterprise_ssc`, `opal_v1`, `opal_v2`, `ruby_ssc`, `pyrite_v1`, `pyrite_v2` | Feature codes **0x0100**, **0x0200**, **0x0203**, **0x0304**, **0x0302**, **0x0303** |
| `desc`                                                                       | Built string from flags                                                              |

---

## `ScsiModePagesInfo` (`scsi_modes`)

`IOCTL_SCSI_PASS_THROUGH`, CDB **0x1A** / page **0x01** (caching).

| Member                      | Mode page bytes        |
| --------------------------- | ---------------------- |
| `valid`                     | Sense + data parse OK  |
| `write_cache`, `read_cache` | Page byte 2 bits       |
| `awre`, `arre`              | Page byte 2 bits 7 / 6 |
| `error_recovery`            | Page byte 3            |

---

## `DevPathInfo` (`devpath`)

`query_devpath`: `SetupDiGetClassDevs` disk class, registry strings.

| Member          | API                                |
| --------------- | ---------------------------------- |
| `valid`         | Match found                        |
| `device_path`   | `SPDRP_DEVICEDESC` / instance path |
| `friendly_name` | `SPDRP_FRIENDLYNAME`               |
| `hw_id`         | `SPDRP_HARDWAREID`                 |
| `location`      | `SPDRP_LOCATION_INFORMATION`       |
| `driver`        | `SPDRP_DRIVER`                     |

---

## `VolumesInfo` (`volumes`) / `VolumeInfo`

`query_vols`: `IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS` per `A:`–`Z:`; `GetVolumeInformationA`; `GetVolumeNameForVolumeMountPointA`.

| Member             | Source                                                 |
| ------------------ | ------------------------------------------------------ |
| `count`, `vols[]`  | Extent `DiskNumber` matches drive index                |
| `letter`           | Drive letter                                           |
| `serial`           | `GetVolumeInformation` volume serial                   |
| `fs_name`, `label` | Same                                                   |
| `guid_path`        | `GetVolumeNameForVolumeMountPoint`                     |
| `mount_point`      | Not used on Windows path (struct cleared; letter used) |

---

<p align="center"><a href="README.md"><b>↑ Back to README</b></a></p>
