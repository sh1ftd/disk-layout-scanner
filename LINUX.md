# Linux - `DiskInfo` extraction reference

<p align="center">
<a href="README.md"><b>↑ Back to README</b></a>
&nbsp;·&nbsp;
<a href="WINDOWS.md"><b>Windows reference →</b></a>
</p>

Enumeration: `/sys/block` (`sd*`, `vd*`, `xvd*`, `hd*`, `nvme*n*`). SATA-style vs NVMe: `scan_sata_disk` vs `scan_nvme_disk` in `queries_linux.c`. Structs: `disk_info.h`.

`scan_disks` then always calls `scan_diskstats` and `scan_mounts` for that block name.

---

## Legend

| Cell    | Meaning                                  |
| ------- | ---------------------------------------- |
| **Yes** | Written when the probe succeeds          |
| **-**   | Left zero / empty (probe not applicable) |
| **No**  | No code path for this OS                 |

---

## Top-level (`DiskInfo`)

| Member         | SATA-style                      | NVMe           |
| -------------- | ------------------------------- | -------------- |
| `drive_number` | Enumerate order (0-based index) | Same           |
| `dev_path`     | `/dev/sdX` etc.                 | `/dev/nvmeXnY` |

---

## `StoragePropInfo` (`storage`)

| Member                 | SATA-style                                                  | NVMe                           |
| ---------------------- | ----------------------------------------------------------- | ------------------------------ |
| `valid`                | **Yes** (set at start of `scan_sata_disk`)                  | **Yes**                        |
| `vendor`, `revision`   | sysfs `device/vendor`, `device/rev`                         | **-** (not read)               |
| `product`              | sysfs `device/model`                                        | sysfs `device/model`           |
| `serial`               | sysfs; else backfill from IDENTIFY                          | sysfs `device/serial`          |
| `bus_type`, `bus_name` | sysfs `device/transport` → SATA/SAS/USB guess; default SATA | Fixed NVMe (**17**) / `"NVMe"` |
| `removable`            | **No** (never set)                                          | **No**                         |

---

## `AtaIdentifyInfo` (`ata`)

| Member                      | SATA-style                                                                           | NVMe                                 |
| --------------------------- | ------------------------------------------------------------------------------------ | ------------------------------------ |
| All IDENTIFY-derived fields | **Yes** via `HDIO_GET_IDENTITY` + `parse_ata_identify` (same word decode as Windows) | **-** (`AtaIdentifyInfo` not filled) |

---

## `DeviceIdsInfo` (`ids`)

| Member            | SATA-style | NVMe   |
| ----------------- | ---------- | ------ |
| _(entire struct)_ | **No**     | **No** |

---

## `GeometryInfo` (`geometry`)

| Member                                                  | SATA-style                    | NVMe        |
| ------------------------------------------------------- | ----------------------------- | ----------- |
| `valid`                                                 | **Yes** if `sysfs size` > 0   | **Yes**     |
| `total_bytes`                                           | `size` × 512 (see code)       | Same        |
| `bytes_per_sector`                                      | `queue/hw_sector_size` or 512 | 512 in code |
| `media_type`, `cylinders`, `heads`, `sectors_per_track` | **No** (remain 0)             | **No**      |

---

## `SmartInfo` (`smart`)

| Member                                            | SATA-style                                                              | NVMe                 |
| ------------------------------------------------- | ----------------------------------------------------------------------- | -------------------- |
| `valid` + attrs + derived temp/POH/cycles/realloc | **Yes** - SG `ATA PASS-THROUGH`, SMART READ VALUES (`parse_smart_data`) | **-**                |
| _(NVMe health is separate struct)_                | -                                                                       | see `NvmeHealthInfo` |

---

## `SmartThreshInfo` (`smart_thresh`)

| Member            | SATA-style | NVMe   |
| ----------------- | ---------- | ------ |
| _(entire struct)_ | **No**     | **No** |

---

## `NvmeHealthInfo` (`nvme_health`)

| Member                                            | SATA-style | NVMe                                                                                                            |
| ------------------------------------------------- | ---------- | --------------------------------------------------------------------------------------------------------------- |
| All fields (critical_warning … error_log_entries) | **-**      | **Yes** - `NVME_IOCTL_ADMIN_CMD`, opcode **0x02**, log **0x02**, 512-byte buffer (same field layout as Windows) |

---

## `NvmeIdentifyInfo` (`nvme_id`) / `NvmeFwSlotInfo` (`nvme_fw`)

| Member             | SATA-style | NVMe   |
| ------------------ | ---------- | ------ |
| _(entire structs)_ | **No**     | **No** |

---

## `CacheInfo` (`cache`) / `ExtraPropsInfo` (`extra`)

| Member                                   | SATA-style                    | NVMe                              |
| ---------------------------------------- | ----------------------------- | --------------------------------- |
| `cache.*`                                | **No**                        | **No**                            |
| `extra.valid`                            | **Yes**                       | **Yes**                           |
| `extra.has_seek_penalty`                 | sysfs `queue/rotational`      | Forced **0** (no rotational read) |
| `extra.trim_enabled`                     | `queue/discard_max_bytes` > 0 | Same                              |
| `extra.align_byte_offset`, `align_valid` | **No**                        | **No**                            |

---

## `PerfInfo` (`perf`)

| Member                                                              | SATA-style                                     | NVMe    |
| ------------------------------------------------------------------- | ---------------------------------------------- | ------- |
| `valid`, `read_count`, `write_count`, `bytes_read`, `bytes_written` | **Yes** - `/proc/diskstats` row for block name | **Yes** |
| `read_time_ns`, `write_time_ns`                                     | **Yes** (ticks × 1e6)                          | **Yes** |
| `idle_time_ns`, `queue_depth`                                       | **No** (remain 0)                              | **No**  |

---

## `DriveLayoutInfo` (`layout`)

| Member            | SATA-style | NVMe   |
| ----------------- | ---------- | ------ |
| _(entire struct)_ | **No**     | **No** |

---

## `RawMbrInfo` (`mbr`)

| Member                                        | SATA-style                    | NVMe   |
| --------------------------------------------- | ----------------------------- | ------ |
| `valid`, `disk_sig`, `boot_sig`, `part1_type` | **Yes** - `pread(fd, 512, 0)` | **No** |

---

## `AtaSecurityInfo`, `PowerModeInfo`, `HpaInfo`, `DcoInfo`, `SedOpalInfo`, `ScsiModePagesInfo`

| Struct | SATA-style | NVMe   |
| ------ | ---------- | ------ |
| All    | **No**     | **No** |

---

## `DevPathInfo` (`devpath`)

| Member                               | SATA-style                             | NVMe             |
| ------------------------------------ | -------------------------------------- | ---------------- |
| `valid`                              | **Yes**                                | **Yes**          |
| `device_path`                        | Same as `dev_path`                     | Same             |
| `driver`                             | sysfs `device/driver` symlink basename | **-** (not read) |
| `friendly_name`, `hw_id`, `location` | **No**                                 | **No**           |

---

## `VolumesInfo` (`volumes`) / `VolumeInfo`

| Member                   | SATA-style                                                          | NVMe    |
| ------------------------ | ------------------------------------------------------------------- | ------- |
| `count`                  | **Yes** - `/proc/mounts` lines whose source block matches this disk | **Yes** |
| `fs_name`, `mount_point` | **Yes**                                                             | **Yes** |
| `letter`, `guid_path`    | **-** (0 / empty)                                                   | **-**   |
| `serial`, `label`        | **-**                                                               | **-**   |

---

<p align="center">
<a href="README.md"><b>↑ Back to README</b></a>
&nbsp;·&nbsp;
<a href="WINDOWS.md"><b>Windows reference →</b></a>
</p>
