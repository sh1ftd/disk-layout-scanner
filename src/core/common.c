#include "common.h"

void print_section(const char* title) {
    printf("\n  -- %s --\n", title);
}

const char* bus_type_name(int t) {
    switch (t) {
        case 1:  return "SCSI";
        case 2:  return "ATAPI";
        case 3:  return "ATA";
        case 6:  return "Fibre";
        case 7:  return "USB";
        case 8:  return "RAID";
        case 9:  return "iSCSI";
        case 10: return "SAS";
        case 11: return "SATA";
        case 12: return "SD";
        case 13: return "MMC";
        case 14: return "Virtual";
        case 16: return "Spaces";
        case 17: return "NVMe";
        case 18: return "SCM";
        case 19: return "UFS";
        default: return "Unknown";
    }
}

const char* id_type_name(int type) {
    switch (type) {
        case 0: return "VendorSpecific";
        case 1: return "VendorId(T10)";
        case 2: return "EUI-64";
        case 3: return "NAA";
        case 4: return "RelTargetPort";
        case 5: return "TargetPortGroup";
        case 6: return "LogicalUnitGroup";
        case 7: return "MD5_LU";
        case 8: return "ScsiNameString";
        default: return "Unknown";
    }
}

const char* codeset_name(int cs) {
    switch (cs) {
        case 1: return "Binary";
        case 2: return "ASCII";
        case 3: return "UTF-8";
        default: return "Reserved";
    }
}

void ata_words_to_string(const WORD* words, int start, int end, char* out, int out_sz) {
    int len = 0;
    for (int w = start; w <= end && len + 2 < out_sz; w++) {
        out[len++] = (char)(words[w] >> 8);
        out[len++] = (char)(words[w] & 0xFF);
    }
    out[len] = '\0';
    while (len > 0 && out[len - 1] == ' ') out[--len] = '\0';
}

const char* smart_attr_name(UINT8 id) {
    switch (id) {
        case 0x01: return "Read Error Rate";
        case 0x02: return "Throughput Performance";
        case 0x03: return "Spin Up Time";
        case 0x04: return "Start/Stop Count";
        case 0x05: return "Reallocated Sectors";
        case 0x07: return "Seek Error Rate";
        case 0x09: return "Power-On Hours";
        case 0x0A: return "Spin Retry Count";
        case 0x0B: return "Calibration Retry";
        case 0x0C: return "Power Cycle Count";
        case 0xAA: return "Available Reserved Space";
        case 0xAB: return "Program Fail Count";
        case 0xAC: return "Erase Fail Count";
        case 0xAD: return "Wear Leveling Count";
        case 0xAE: return "Unexpected Power Loss";
        case 0xB7: return "SATA Downshift Count";
        case 0xB8: return "End-to-End Error";
        case 0xBB: return "Uncorrectable Errors";
        case 0xBE: return "Airflow Temperature";
        case 0xC0: return "Power-Off Retract";
        case 0xC2: return "Temperature";
        case 0xC3: return "Hardware ECC Recovered";
        case 0xC4: return "Reallocation Events";
        case 0xC5: return "Pending Sectors";
        case 0xC6: return "Offline Uncorrectable";
        case 0xC7: return "UDMA CRC Errors";
        case 0xF1: return "Total LBAs Written";
        case 0xF2: return "Total LBAs Read";
        case 0xFE: return "Free Fall Events";
        default:   return "";
    }
}

const char* form_factor_name(UINT8 ff) {
    switch (ff) {
        case 1:  return "5.25\"";
        case 2:  return "3.5\"";
        case 3:  return "2.5\"";
        case 4:  return "1.8\"";
        case 5:  return "<1.8\"";
        case 6:  return "mSATA";
        case 7:  return "M.2";
        case 8:  return "MicroSSD";
        case 9:  return "CFast";
        default: return "Unknown";
    }
}

const char* ata_version_name(UINT16 major) {
    if (major & (1 << 11)) return "ACS-4";
    if (major & (1 << 10)) return "ACS-3";
    if (major & (1 << 9))  return "ACS-2";
    if (major & (1 << 8))  return "ATA8-ACS";
    if (major & (1 << 7))  return "ATA/ATAPI-7";
    if (major & (1 << 6))  return "ATA/ATAPI-6";
    if (major & (1 << 5))  return "ATA/ATAPI-5";
    return "Unknown";
}
