# disk-layout-scanner

Command-line program that enumerates **physical** disks and prints a report to the terminal (text or JSON) or to an HTML file. It extracts drive identity, partitions, SMART, NVMe health, and related fields the OS can expose.

---

## Requirements

- **Windows:** Administrator (SMART / pass-through / `\\.\PhysicalDrive*`).
- **Linux:** root (block dev ioctls); kernel-dependent.

---

## Collected data (summary)

`scan_disks` fills `DiskInfo`; formatters omit sections with no valid data. Full probe list: `queries_win.c`, `queries_linux.c`.

### Windows (`\\.\PhysicalDriveN`)

**→** [Field-level reference (Windows)](WINDOWS.md)

| Probe / API surface                                             | Populated fields (high level)                                                                                                                                                                                           |
| --------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `IOCTL_STORAGE_QUERY_PROPERTY` → `StorageDeviceProperty`        | Vendor, product, revision, serial, `STORAGE_BUS_TYPE`, removable                                                                                                                                                        |
| `IOCTL_ATA_PASS_THROUGH` → IDENTIFY                             | `AtaIdentifyInfo`: model/serial/fw, LBA28/48, WWN, RPM/SSD, queue depth, SATA gen, NCQ/TRIM/LBA48/write-cache/SMART capability flags, logical/physical sector layout, form factor, ATA version, UDMA, `transport_major` |
| `SMART_SEND_DRIVE_COMMAND` + `SMART_RCV_DRIVE_DATA`             | `SmartInfo` (attributes + derived temp/POH/cycles/realloc); `SmartThreshInfo` (threshold page **0xD1**)                                                                                                                 |
| `IOCTL_STORAGE_QUERY_PROPERTY` → NVMe protocol                  | `NvmeHealthInfo` (log **0x02**); `NvmeIdentifyInfo`; `NvmeFwSlotInfo`                                                                                                                                                   |
| `IOCTL_DISK_GET_DRIVE_LAYOUT_EX`                                | `DriveLayoutInfo`: MBR/GPT/other, disk signature or disk GUID, partition offset/length/type or GPT name                                                                                                                 |
| `IOCTL_DISK_GET_DRIVE_GEOMETRY_EX` + `IOCTL_DISK_PERFORMANCE`   | `GeometryInfo` (incl. media type, CHS); `PerfInfo` (bytes, counts, times, queue depth)                                                                                                                                  |
| `IOCTL_DISK_GET_CACHE_INFORMATION` + property IDs **6 / 7 / 8** | `CacheInfo`; `ExtraPropsInfo` (alignment, seek penalty, TRIM)                                                                                                                                                           |
| `IOCTL_STORAGE_QUERY_PROPERTY` → `StorageDeviceIdProperty`      | `DeviceIdsInfo` (typed identifiers, hex/ASCII)                                                                                                                                                                          |
| `ReadFile` LBA **0** (512 B)                                    | `RawMbrInfo`                                                                                                                                                                                                            |
| Additional ATA pass-through                                     | `AtaSecurityInfo` (IDENTIFY word 128); `PowerModeInfo`; `HpaInfo`; `DcoInfo`                                                                                                                                            |
| Trusted receive / security scan                                 | `SedOpalInfo`                                                                                                                                                                                                           |
| SCSI MODE SENSE (**0x1A** / **0x01**)                           | `ScsiModePagesInfo`                                                                                                                                                                                                     |
| SetupAPI disk instance                                          | `DevPathInfo` (instance path, friendly name, HW ID, location, driver)                                                                                                                                                   |
| Volume IOCTLs + `GetVolumeInformation`                          | `VolumesInfo` (letter, FS, label, serial, `\\?\Volume{…}`)                                                                                                                                                              |

**→** [Field-level reference (Windows)](WINDOWS.md)

### Linux (`/sys/block` → `sd*` / `vd*` / `xvd*` / `hd*` / `nvme*n*`)

**→** [Field-level reference (Linux)](LINUX.md)

| Data class                                                                              | SATA-style (`sd*`, `vd*`, …)                                                             | NVMe (`nvme*n`)                                                  |
| --------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------- | ---------------------------------------------------------------- |
| **Storage identity**                                                                    | sysfs `vendor` / `model` / `rev`; `transport` → bus guess; serial from sysfs or IDENTIFY | sysfs `model` / `serial` / `firmware_rev`; bus fixed NVMe        |
| **ATA IDENTIFY**                                                                        | `HDIO_GET_IDENTITY` → same `AtaIdentifyInfo` parse as Windows                            | Not used                                                         |
| **SMART / health**                                                                      | SG **ATA PASS-THROUGH**, SMART READ VALUES → `SmartInfo` (no threshold page)             | `NVME_IOCTL_ADMIN_CMD`, get-log **0x02** → `NvmeHealthInfo` only |
| **NVMe Identify / FW slots**                                                            | -                                                                                        | Not implemented                                                  |
| **Partition layout**                                                                    | Not implemented                                                                          | Not implemented                                                  |
| **Mounts**                                                                              | `/proc/mounts` → `fs_name`, `mount_point`                                                | Same                                                             |
| **Geometry**                                                                            | sysfs `size`, `queue/hw_sector_size` → `GeometryInfo` (no CHS / media type)              | Same                                                             |
| **I/O counters**                                                                        | `/proc/diskstats` → `PerfInfo`                                                           | Same                                                             |
| **Queue / discard**                                                                     | `queue/rotational`, `queue/discard_max_bytes` → `ExtraPropsInfo`                         | `discard_max_bytes` only; non-rotating assumed                   |
| **LBA0**                                                                                | `pread(512)` → `RawMbrInfo`                                                              | Not read                                                         |
| **Device path**                                                                         | `dev_path`; sysfs `device/driver` → `DevPathInfo.driver`                                 | `dev_path` only                                                  |
| **Security / HPA / DCO / SED / SCSI mode pages / `StorageDeviceIdProperty` / SetupAPI** | Not implemented                                                                          | Not implemented                                                  |

**→** [Field-level reference (Linux)](LINUX.md)
