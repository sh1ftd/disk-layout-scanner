# disk-layout-scanner

Walks **physical** drives (up to **64** per run) and dumps identity, health, partitioning, and OS-visible volume hooks into one report. Not a benchmarker and not forensic-grade—values come from firmware, drivers, and OS APIs.

**Typical use (Windows):** run the `.exe` **as Administrator** (double-click or otherwise). It writes **HTML** beside the binary and opens it in your default browser—no flags required. **JSON** and plain **text** modes exist for scripting and piping.

**Linux:** run the binary with the privileges your block devices need; default is still HTML to a file in the working directory (browser open where `xdg-open` / similar exists).

| | Windows | Linux |
|--|---------|-------|
| Access | Admin, `\\.\PhysicalDrive0`… | root where raw `ioctl` / device nodes are required |
| Scope | Full probe set below | Subset; missing probes leave sections empty |

### Windows (full)

- **Storage descriptor** — vendor / product / firmware / serial, bus type, removable flag  
- **ATA IDENTIFY** — model, serial, capacity, WWN/NAA/OUI, media hints, SATA link, NCQ/TRIM, SMART flags  
- **SMART** — health summary, attributes, thresholds  
- **NVMe** — health log, Identify Controller, firmware slots  
- **SCSI** — VPD page 83 IDs, mode pages (cache, realloc, error recovery)  
- **Layout** — MBR or GPT, partition table with offsets/sizes/types (GPT names)  
- **Raw MBR** — disk signature, boot signature  
- **Geometry** — size, bytes/sector, CHS-style fields  
- **Security / power / clipping** — ATA security state, power mode, HPA, DCO, SED hints  
- **Cache & perf** — write/read cache policy, SMART-style perf counters  
- **PnP path** — instance ID, friendly name, hardware ID, location  
- **Volumes** — drive letters, volume serial, FS, label, mount paths  

### Linux (current)

- Block device identity via **sysfs** (model, serial, WWN where exposed)  
- **ATA IDENTIFY** when the path exposes it  
- **SMART** via SAT passthrough where supported  
- **NVMe** health log  
- **MBR** first-sector fields, **geometry** from sysfs  
- **`/proc/diskstats`** counters, **`/proc/mounts`** hooks for mounted paths  
- Basic **device path** string where available  

Prebuilt **Windows** + static **Linux (musl)** binaries: [Releases](https://github.com/sh1ftd/disk-layout-scanner/releases).

MIT License.
