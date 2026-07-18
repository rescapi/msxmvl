// msxmvl extension: mini-diskOS — a precomputed sector index instead of a FAT walk.
//
// A boot disk built by tools/mkbootdsk.sh places every declared entry CONTIGUOUSLY
// on the disk and records, at BUILD time, where each one landed. That table — the
// "sector index" — lets a payload pull a data blob straight off the disk by a small
// numeric id, with no FAT parsing, no directory search, no MSX-DOS: the work a file
// system does at runtime is done once, by the build tool. That is the whole trick,
// and why this is smaller and faster than opening a file through DOS.
//
// The image stays a VALID FAT12 disk: every entry is also a real, listable file, so
// the disk mounts host-side for debugging. The sector index is an EXTRA region (the
// unused second half of the boot sector) layered on top; the FAT still describes the
// same clusters. Tradeoff: the index is only correct while the files stay put — edit
// a file host-side and its clusters may move, so rebuild the image with mkbootdsk.sh
// rather than editing files in place.
//
// Sector reads go through DiskDOS_ReadSectors (disk.c), i.e. the disk-ROM kernel
// BDOS absolute-sector call (0x2F) reached via the 0x0005 hook crt0_bootdisk.asm
// installs. Under the boot environment page 0 is RAM, so the raw BIOS PHYDIO entry
// (0x0144) is not addressable — the BDOS door is the one that works, and it is the
// one the DOS raw-sector examples already use.
#ifndef MSXMVL_DISKOS_H
#define MSXMVL_DISKOS_H

#include "types.h"

//=============================================================================
// ON-DISK SECTOR INDEX (written by tools/mkbootdsk.sh)
//=============================================================================
// Stored in sector 0 (the boot sector) starting at byte 0x100 — the half of the
// sector the disk ROM does not copy to RAM at boot, so it costs no boot-code space.
//
//   +0x00  2  magic 'D','K'  (read little-endian as DISKOS_MAGIC)
//   +0x02  1  entry count N
//   +0x03  N * 7 entries, each:
//              +0  1  id
//              +1  2  start sector      (u16, little-endian)
//              +3  2  sector count       (u16)  — contiguous 512-byte sectors
//              +5  2  byte length        (u16)  — exact size of the blob
#define DISKOS_INDEX_SECTOR   0
#define DISKOS_INDEX_OFFSET   0x100
#define DISKOS_MAGIC          0x4B44      // 'D','K' little-endian
#define DISKOS_ENTRY_SIZE     7

//=============================================================================
// FUNCTIONS
//=============================================================================

// Function: DiskOS_Load
// Load the blob with the given id into `dst` (must be RAM, >= DiskOS_Length(id)
// bytes, rounded up to 512). Reads its contiguous sectors directly.
//
// Return:
//   0 on success, 0xFF if no entry has that id, otherwise the disk error code.
u8 DiskOS_Load(u8 id, void* dst);

// Function: DiskOS_Length
// Exact byte length of the blob with the given id, or 0 if there is no such id.
u16 DiskOS_Length(u8 id);

// Function: DiskOS_Sectors
// Number of 512-byte sectors the blob occupies, or 0 if there is no such id.
u16 DiskOS_Sectors(u8 id);

#endif // MSXMVL_DISKOS_H
