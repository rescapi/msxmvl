// msxmvl extension: raw disk sector I/O via the BIOS PHYDIO entry (0x0144).
//
// This is the layer BELOW MSX-DOS. It talks to the disk interface ROM directly, so
// it needs no MSX-DOS, no FAT, no file system: just "give me sector N". Use it for
// disk utilities (sector editors, copiers, format tools), custom on-disk formats,
// and loaders that pull levels straight off a track without a DOS in memory.
//
// It DOES need a disk interface ROM in the machine, and that ROM must already be
// initialized -- see the note in disk.c about calling this from a cartridge.
#ifndef MSXMVL_DISK_H
#define MSXMVL_DISK_H

#include "types.h"

//=============================================================================
// MEDIA DESCRIPTORS
//=============================================================================
// The media byte tells the disk ROM the disk geometry. It is also byte 0x15 of
// the boot sector, so you can read it off the disk itself.
#define DISK_MEDIA_720K     0xF9   // 3.5" 80 tracks, 2 sides, 9 sectors
#define DISK_MEDIA_360K     0xF8   // 3.5"/5.25" 80 tracks, 1 side, 9 sectors
#define DISK_MEDIA_320K     0xFA
#define DISK_MEDIA_180K     0xFC

//=============================================================================
// GLOBALS
//=============================================================================
extern u8 g_Disk_Drive;   // 0 = A:, 1 = B:, ...
extern u8 g_Disk_Media;   // media descriptor (default DISK_MEDIA_720K)

//=============================================================================
// FUNCTIONS
//=============================================================================

// Function: Disk_SetDrive
// Select the drive used by the transfer functions (0 = A:).
inline void Disk_SetDrive(u8 drive) { g_Disk_Drive = drive; }

// Function: Disk_SetMedia
// Set the media descriptor byte (see DISK_MEDIA_*).
inline void Disk_SetMedia(u8 media) { g_Disk_Media = media; }

// Function: Disk_ReadSectors
// Read `count` sectors starting at logical `sector` into `buffer`.
// The buffer must be in RAM and at least count*512 bytes.
//
// Return:
//   0 on success, otherwise the BIOS error code (see DISK_ERR_*).
u8 Disk_ReadSectors(u16 sector, u8 count, void* buffer);

// Function: Disk_WriteSectors
// Write `count` sectors from `buffer` starting at logical `sector`.
//
// Return:
//   0 on success, otherwise the BIOS error code.
u8 Disk_WriteSectors(u16 sector, u8 count, const void* buffer);

//-----------------------------------------------------------------------------
// Under MSX-DOS, use these instead. Still raw sectors (no FAT, no files) -- but
// reached via BDOS 0x2F/0x30, because a .COM has RAM in page 0 and cannot call the
// BIOS at 0x0144 at all. See disk.c.

// Function: DiskDOS_ReadSectors
// Read `count` sectors from `sector` into `buffer`. Returns 0 on success.
u8 DiskDOS_ReadSectors(u16 sector, u8 count, void* buffer);

// Function: DiskDOS_WriteSectors
// Write `count` sectors from `buffer`. Returns 0 on success.
u8 DiskDOS_WriteSectors(u16 sector, u8 count, const void* buffer);

//=============================================================================
// ERROR CODES (as returned by the disk ROM in A)
//=============================================================================
#define DISK_ERR_WRITE_PROTECT  0
#define DISK_ERR_NOT_READY      2
#define DISK_ERR_DATA           4
#define DISK_ERR_SEEK           6
#define DISK_ERR_RECORD_NOT_FND 8
#define DISK_ERR_WRITE_FAULT    10
#define DISK_ERR_OTHER          12

#endif // MSXMVL_DISK_H
