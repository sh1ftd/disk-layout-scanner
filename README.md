# disk-layout-scanner

## What it is

Command-line program that enumerates **physical** disks and prints a report to the terminal (text or JSON) or to an HTML file—drive identity, partitions, SMART, NVMe health, and related fields the OS can expose.

---

## Requirements

- **Windows:** Administrator (SMART / pass-through / `\\.\PhysicalDrive*`).
- **Linux:** root (block dev ioctls); kernel-dependent.

---

## What gets reported

Empty sections are omitted when there is nothing to show.

| Area | Fields |
|------|--------|
| Index | drive # |
| Storage | vendor, product, revision, serial, bus type/name, removable **(Win)** |
| ATA IDENTIFY | model/serial/fw, ATA version, LBA28/48 + GB, WWN/NAA/OUI, SSD/RPM, form factor, cache KB, logical sector + phys ratio, SATA gen, queue depth, NCQ/TRIM/LBA48/write cache/SMART flags, UDMA **(JSON)** |
| Geometry | total bytes, B/s, CHS **(Win; CHS 0 on Linux)** |
| SMART | pass/fail, temp/POH/cycles/realloc when derivable, all attrs id/name/cur/worst/raw |
| SMART thresholds | id, threshold, exceeded **(Win)** |
| VPD 0x83 | id entries: type, codeset **(HTML)**, ascii/hex |
| Layout | MBR/GPT, disk sig or disk GUID, part count; each part: #, offset, len, MBR type or GPT type+name **(Win)** |
| Raw LBA0 | disk sig, boot 0xAA55, first part type |
| NVMe health | critical warn, temp (K json / °C text), spare%+thresh **(not thresh in json)**, %used, data read/write, POH, unsafe shutdowns, media errs *(host cmd counts + err-log count read but not printed)* |
| NVMe Identify | serial/model/fw, VID/SSVID, ctrl id, ver, OUI, NS count, total NVM, MDTS **(Win; HTML drops ctrl id+MDTS)** |
| NVMe FW slots | active, pending **(text/json)**, rev per slot **(Win)** |
| Cache IOCTL | read/write cache, write-through, power protect **(Win)** |
| Extra IOCTL | seek-penalty hint, TRIM, align offset **(Win)** |
| Perf | rd/wr count + bytes, QD **(Win IOCTL; Linux diskstats)** *(Linux rd/wr time not shown)* |
| ATA security / power / HPA / DCO | security state, power mode, HPA/DCO sizes+flags **(Win)** |
| SED/Opal | capable, locked, summary `desc`; JSON also opal v1/v2 + enterprise **(Win)** |
| SCSI mode pg | wr/rd cache, AWRE/ARRE, retry byte **(Win)** |
| Dev path | instance, friendly, HW id, location **(Win)** |
| Volumes | letter+vol serial+FS+label+GUID **(Win)** · Linux: mount+FS in text/html; **JSON omits mount path** |

**Linux today:** sysfs id, `HDIO_GET_IDENTITY`, SMART via SG, size/sector geometry, raw MBR, rotational/discard, diskstats, mounts, NVMe **health log only**—no layout API, VPD, SMART thresh, NVMe identify/fw, cache/extra IOCTLs, ATA sec/power/HPA/DCO/SED/SCSI mode, PnP path, CHS, Windows-style volumes.
