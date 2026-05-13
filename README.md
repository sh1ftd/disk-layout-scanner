# disk-layout-scanner

## What it is

Command-line program that enumerates **physical** disks and prints a report to the terminal (text or JSON) or to an HTML file—drive identity, partitions, SMART, NVMe health, and related fields the OS can expose.

---

## Requirements

- **Windows:** Administrator (SMART / pass-through / `\\.\PhysicalDrive*`).
- **Linux:** root (block dev ioctls); kernel-dependent.

---

## Extraction inventory

Output hides a block when that probe failed or stayed empty. This section lists what **queries try to fill** (`queries_win.c`, `queries_linux.c`).

### Windows (each `\\.\PhysicalDriveN` that opens)

- **Enumerate:** `drive_number`, `dev_path`.
- **`IOCTL_STORAGE_QUERY_PROPERTY` / `StorageDeviceProperty`:** vendor, product, revision, serial, bus type + name, removable.
- **`IOCTL_ATA_PASS_THROUGH` / IDENTIFY:** full `AtaIdentifyInfo` (model/serial/fw, LBA28/48, capacity GB, WWN + NAA/OUI/vendor-specific, RPM/SSD, cache KB, queue depth, SATA gen, NCQ/TRIM/LBA48/write cache/SMART flags, logical sector + phys ratio, form factor, ATA major, UDMA, **`transport_major`**).
- **`IOCTL_STORAGE_QUERY_PROPERTY` / `StorageDeviceIdProperty`:** up to 16 identifiers (type, code set, names, size, hex + ASCII).
- **`IOCTL_DISK_GET_DRIVE_GEOMETRY_EX`:** total bytes, bytes/sector, **media type**, cylinders, heads, sectors/track.
- **SMART:** `SMART_SEND_DRIVE_COMMAND` enable + return-status (no stored bits); `SMART_RCV_DRIVE_DATA` READ VALUES → `SmartInfo` (attrs id/**flags**/name/current/worst/raw, derived temp/POH/cycles/realloc, default `health_ok`); separate RCV **0xD1** thresholds → `SmartThreshInfo` (id, threshold, exceeded vs current).
- **`IOCTL_STORAGE_QUERY_PROPERTY` / NVMe protocol:** health log **0x02** → all `NvmeHealthInfo` scalars; Identify controller → all `NvmeIdentifyInfo` (incl. **`unalloc_cap_bytes`**); firmware-slot log → `NvmeFwSlotInfo`.
- **`IOCTL_DISK_GET_CACHE_INFORMATION`:** read/write cache, write-through (`CacheInfo`; **`power_protected` is never written** — stays 0).
- **`IOCTL_STORAGE_QUERY_PROPERTY` IDs 6 / 7 / 8:** alignment, seek-penalty, TRIM.
- **`IOCTL_DISK_PERFORMANCE`:** bytes read/write, I/O counts, read/write/**idle** times, queue depth.
- **`IOCTL_DISK_GET_DRIVE_LAYOUT_EX`:** style MBR/GPT/other, MBR signature or disk GPT GUID, partition count; each nonempty partition → offset, length, number, MBR type **or** GPT type GUID + name.
- **Read 512 @ LBA0:** `RawMbrInfo` (disk sig, boot sig, first MBR part type).
- **IDENTIFY word 128:** `AtaSecurityInfo` (supported/enabled/locked/frozen/**count_expired**/enhanced erase/master pwd cap).
- **CHECK POWER MODE:** `PowerModeInfo`.
- **READ NATIVE MAX (EXT):** `HpaInfo` vs IDENTIFY max LBA.
- **DCO IDENTIFY:** `DcoInfo`.
- **Security protocol IN (Opal-style):** `SedOpalInfo` (SED capable, Opal v1/v2, Enterprise, Ruby, Pyrite v1/v2, locked, `desc`).
- **SCSI MODE SENSE (0x1A / 0x01):** `ScsiModePagesInfo`.
- **SetupAPI:** `DevPathInfo` (instance path, friendly name, hardware ID, location, **driver**).
- **Volumes A–Z (extents on this disk):** letter, volume serial, label, FS name, `\\?\Volume{…}` path.

### Linux (`/sys/block` → `sd*` / `vd*` / `xvd*` / `hd*` / `nvme*n*`)

Linux uses two code paths (`scan_sata_disk` vs `scan_nvme_disk`). Same binary; fewer probes than Windows.

| Source | SATA-style disks (`sd*`, `vd*`, …) | NVMe (`nvme*n`) |
|--------|--------------------------------------|-----------------|
| **sysfs** | `vendor`, `model`, `rev`, `transport`→bus guess, `size`, `queue/hw_sector_size`, `queue/rotational`, `queue/discard_max_bytes`, `device/driver`→module name | `device/model`, `serial`, `firmware_rev`, `size`, `discard_max_bytes` |
| **ioctl** | `HDIO_GET_IDENTITY` → full `AtaIdentifyInfo`; SG **ATA pass-through** → SMART READ VALUES → `SmartInfo`; `pread` sector 0 → `RawMbrInfo` | `NVME_IOCTL_ADMIN_CMD` → health log **0x02** → `NvmeHealthInfo` |
| **proc** | `/proc/diskstats` → `PerfInfo`; `/proc/mounts` → `fs_name` + `mount_point` | same `diskstats` + `mounts` |

Both paths set `drive_number`, `dev_path`, and mark `extra` (TRIM from discard sysfs; NVMe forces “no seek penalty”). Storage **`removable`** is not read on Linux.

**Not implemented on Linux** (no code path; Windows has these):

| Area |
|------|
| OS partition layout (`IOCTL_DISK_GET_DRIVE_LAYOUT_EX` equivalent) |
| Storage device ID descriptor (`StorageDeviceIdProperty`) |
| SMART threshold page **0xD1** |
| NVMe Identify controller + firmware-slot log |
| Disk cache IOCTL + storage property IDs **6 / 7 / 8** (align, seek penalty, TRIM at that layer) |
| ATA security / power mode / HPA / DCO / SED scan / SCSI mode sense |
| SetupAPI-style PnP strings (friendly name, HW ID, location) beyond `dev_path` + driver (SATA only) |
| Geometry **media type** and **CHS** from kernel |
| Windows-style volumes (drive letter, volume GUID path, volume serial from `GetVolumeInformation`) |
