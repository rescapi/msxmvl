// msxmvl — clean-room reimplementation of MSXgl "dos" module public API.
// Interface (names, signatures, documented behaviour) taken from MSXgl's
// engine/src/dos.h header ONLY. No MSXgl implementation source was read.
//
// All wrappers issue MSX-DOS BDOS calls (entry 0x0005) or, for the inter-slot
// helpers, the DOS-callable BIOS entries (RDSLT 0x000C, WRSLT 0x0014,
// CALSLT 0x001C). The header declares NO __PRESERVES() contract on any DOS
// function, so we follow the standard SDCC ABI (AF/BC/DE/HL/IX clobberable)
// and additionally guard IY (SDCC's frame pointer) across every BDOS/BIOS call
// since MSX-DOS/BIOS do not promise to preserve it.
#ifndef MSXMVL_DOS_H
#define MSXMVL_DOS_H

#include "types.h"

//=============================================================================
// CALLING-CONVENTION MACROS (mirror MSXgl core.h)
//=============================================================================
#define __FASTCALL      __z88dk_fastcall
#define __CALLEE        __z88dk_callee
#ifndef __PRESERVES
#define __PRESERVES     __preserves_regs
#endif

// Callback default signature (mirror MSXgl core.h)
typedef void (*callback)(void);

//=============================================================================
// CONFIG (defaults mirror MSXgl config_default.h — everything enabled)
//=============================================================================
#ifndef DOS_USE_FCB
#define DOS_USE_FCB             TRUE
#endif
#ifndef DOS_USE_HANDLE
#define DOS_USE_HANDLE          TRUE
#endif
#ifndef DOS_USE_UTILITIES
#define DOS_USE_UTILITIES       TRUE
#endif
#ifndef DOS_USE_VALIDATOR
#define DOS_USE_VALIDATOR       TRUE
#endif
#ifndef DOS_USE_ERROR_HANDLER
#define DOS_USE_ERROR_HANDLER   TRUE
#endif
#ifndef DOS_USE_BIOSCALL
#define DOS_USE_BIOSCALL        TRUE
#endif

// BDOS entry point (MSX-DOS TPA)
#define BDOS                    0x0005
#define DOS_TPA                 0x0006

//=============================================================================
// MSX-DOS ROUTINE NUMBERS (subset actually issued by this module)
//=============================================================================
#define DOS_FUNC_TERM0          0x00
#define DOS_FUNC_CONIN          0x01
#define DOS_FUNC_CONOUT         0x02
#define DOS_FUNC_STROUT         0x09
#define DOS_FUNC_CPMVER         0x0C
#define DOS_FUNC_SELDSK         0x0E
#define DOS_FUNC_FOPEN          0x0F
#define DOS_FUNC_FCLOSE         0x10
#define DOS_FUNC_SFIRST         0x11
#define DOS_FUNC_SNEXT          0x12
#define DOS_FUNC_FDEL           0x13
#define DOS_FUNC_RDSEQ          0x14
#define DOS_FUNC_WRSEQ          0x15
#define DOS_FUNC_FMAKE          0x16
#define DOS_FUNC_LOGIN          0x18
#define DOS_FUNC_CURDRV         0x19
#define DOS_FUNC_SETDTA         0x1A
#define DOS_FUNC_ALLOC          0x1B
#define DOS_FUNC_GDATE          0x2A
#define DOS_FUNC_GTIME          0x2C
#define DOS_FUNC_WRBLK          0x26
#define DOS_FUNC_RDBLK          0x27
#define DOS_FUNC_DPARM          0x31
#define DOS_FUNC_FFIRST         0x40
#define DOS_FUNC_FNEXT          0x41
#define DOS_FUNC_OPEN           0x43
#define DOS_FUNC_CREATE         0x44
#define DOS_FUNC_CLOSE          0x45
#define DOS_FUNC_ENSURE         0x46
#define DOS_FUNC_DUP            0x47
#define DOS_FUNC_READ           0x48
#define DOS_FUNC_WRITE          0x49
#define DOS_FUNC_SEEK           0x4A
#define DOS_FUNC_DELETE         0x4D
#define DOS_FUNC_RENAME         0x4E
#define DOS_FUNC_MOVE           0x4F
#define DOS_FUNC_ATTR           0x50
#define DOS_FUNC_HDELETE        0x52
#define DOS_FUNC_HRENAME        0x53
#define DOS_FUNC_HMOVE          0x54
#define DOS_FUNC_HATTR          0x55
#define DOS_FUNC_GETCD          0x59
#define DOS_FUNC_CHDIR          0x5A
#define DOS_FUNC_TERM           0x62
#define DOS_FUNC_EXPLAIN        0x66
#define DOS_FUNC_DOSVER         0x6F

// BIOS entries callable from MSX-DOS
#define DOS_BIOS_RDSLT          0x000C
#define DOS_BIOS_WRSLT          0x0014
#define DOS_BIOS_CALSLT         0x001C

//=============================================================================
// MISC. RETURN / STATE CONSTANTS
//=============================================================================
#define DOS_ERR_NONE            0x00
#define DOS_SUCCESS             DOS_ERR_NONE
#define HANDLE_INVALID          0xFF

#define DOS_VER_NONE            0
#define DOS_VER_1               1
#define DOS_VER_2               2
#define DOS_VER_NEXTOR          3

// Special control characters
#define CTRL_BEEP               0x07
#define CTRL_BS                 0x08
#define CTRL_TAB                0x09
#define CTRL_LF                 0x0A
#define CTRL_HOME               0x0B
#define CTRL_CLS                0x0C
#define CTRL_RETURN             0x0D

// MSX-DOS 2 Standard descriptors
#define DOS_STDIN               0
#define DOS_STDOUT              1
#define DOS_STDERR              2
#define DOS_AUX                 3
#define DOS_PRN                 4

// MSX-DOS 2 Open/create flags
#define O_RDWR                  0x00
#define O_RDONLY                0x01
#define O_WRONLY                0x02
#define O_INHERIT               0x04

// MSX-DOS 2 Create attributes
#define ATTR_RDONLY             0x01
#define ATTR_HIDDEN             0x02
#define ATTR_SYSTEM             0x04
#define ATTR_VOLUME             0x08
#define ATTR_FOLDER             0x10
#define ATTR_ARCHIVE            0x20
#define ATTR_RESERVED           0x40
#define ATTR_DEVICE             0x80

// MSX-DOS 2 Seek whence
#define SEEK_SET                0
#define SEEK_CUR                1
#define SEEK_END                2

// Drive number
enum DOS_DRIVE
{
	DOS_DRIVE_DEFAULT = 0x00,
	DOS_DRIVE_A       = 0x01,
	DOS_DRIVE_B       = 0x02,
	DOS_DRIVE_C       = 0x03,
	DOS_DRIVE_D       = 0x04,
	DOS_DRIVE_E       = 0x05,
	DOS_DRIVE_F       = 0x06,
	DOS_DRIVE_G       = 0x07,
	DOS_DRIVE_H       = 0x08,
};

#define DRIVE_NUM(d)            ((d) - 'A' + 1)

// Device ID
enum DOS_DEVICE
{
	DOS_DEVICE_DRIVE_A   = 0x40,
	DOS_DEVICE_DRIVE_B   = 0x41,
	DOS_DEVICE_DRIVE_C   = 0x42,
	DOS_DEVICE_DRIVE_D   = 0x43,
	DOS_DEVICE_DRIVE_E   = 0x44,
	DOS_DEVICE_DRIVE_F   = 0x45,
	DOS_DEVICE_DRIVE_G   = 0x46,
	DOS_DEVICE_DRIVE_H   = 0x47,
	DOS_DEVICE_PRINTER   = 0xFB,
	DOS_DEVICE_LIST      = 0xFC,
	DOS_DEVICE_NULL      = 0xFC,
	DOS_DEVICE_AUXILIARY = 0xFE,
	DOS_DEVICE_CONSOLE   = 0xFF,
};

// C-style aliases to manage a file through a handle
#define DOS_FOpen               DOS_OpenHandle
#define DOS_FCreate             DOS_CreateHandle
#define DOS_FClose              DOS_CloseHandle
#define DOS_FEnsure             DOS_EnsureHandle
#define DOS_FDuplicate          DOS_DuplicateHandle
#define DOS_FRead               DOS_ReadHandle
#define DOS_FWrite              DOS_WriteHandle
#define DOS_FSeek               DOS_SeekHandle
#define DOS_FDelete             DOS_DeleteHandle
#define DOS_FRename             DOS_RenameHandle
#define DOS_FMove               DOS_MoveHandle
#define DOS_FSetAttribute       DOS_SetAttributeHandle
#define DOS_FGetAttribute       DOS_GetAttributeHandle

//=============================================================================
// STRUCTURES
//=============================================================================

// File Control Block structure (MSX-DOS 1)
typedef struct DOS_FCB
{
	u8  Drive;          // 0 = default, 1 = A, ... 8 = H
	c8  Name[11];       // 8.3 filename
	u16 CurrentBlock;
	u16 RecordSize;
	u32 Size;
	u16 Date;           // [YYYYYYYM|MMMDDDDD]
	u16 Time;
	u8  DeviceID;
	u8  Directory;
	u16 TopCluster;
	u16 LastCluster;
	u16 RelativeLoc;
	u8  CurrentRecord;
	u32 RandomRecord;
} DOS_FCB;

// File info block (MSX-DOS 2)
typedef struct DOS_FIB
{
	u8  Ident;          //     0 - always 0xFF
	u8  Filename[13];   //  1..13 - ASCIIZ filename
	u8  Attribute;      //    14 - attributes byte
	u16 Time;           // 15..16
	u16 Date;           // 17..18
	u16 Cluster;        // 19..20
	u32 Size;           // 21..24
	u8  Drive;          //    25
	u8  Reserved[38];   // 26..63 - internal
} DOS_FIB;

#if ((DOS_USE_VALIDATOR) || (DOS_USE_ERROR_HANDLER))
// Backup of the last error value
extern u8 g_DOS_LastError;
// Stack pointer saved for the DOS 1 error handler
extern u16 g_DOS_ErrorStack;
#endif

// Backup of the last file info block
extern DOS_FIB g_DOS_LastFIB;

// MSX-DOS version structure
typedef struct DOS_Version
{
	u16 Kernel;
	u16 System;
} DOS_Version;

// MSX-DOS disk information structure
typedef struct DOS_DiskInfo
{
	u8  SectorPerCluster;   // +0
	u16 SectorSize;         // +1 (always 512)
	u16 TotalClusters;      // +3
	u16 FreeClusters;       // +5
} DOS_DiskInfo;

// MSX-DOS 2 time structure
typedef struct DOS_Time
{
	u16 Year;   // +0
	u8  Date;   // +2 (day of week)
	u8  Month;  // +3
	u8  Day;    // +4
	u8  Minute; // +5
	u8  Hour;   // +6
	u8  Second; // +7
} DOS_Time;

// MSX-DOS 2 disk parameters structure (32-byte DPARM buffer)
typedef struct DOS_DiskParam
{
	u8  DriveNum;
	u16 SectorSize;
	u8  ClusterSectors;
	u16 ReservedSectors;
	u8  FATNum;
	u16 RootNum;
	u16 LogicalSectors;
	u8  Media;
	u8  FATSectors;
	u16 RootSector;
	u16 DataSector;
	u16 MaxClusters;
	u8  DirtyFlag;
	u32 VolumeID;
	u8  Reserved[8];
} DOS_DiskParam;

//=============================================================================
// MSX-DOS 1 FUNCTIONS
//=============================================================================

//.............................................................................
// Group: MSX-DOS 1 Core

// Call a BDOS function
void DOS_Call(u8 func);

// Exit program and return to DOS (TERM0, 0x00)
void DOS_Exit0();

//.............................................................................
// Group: MSX-DOS 1 Console IO

// Input character (CONIN, 0x01)
c8 DOS_CharInput();

// Output character (CONOUT, 0x02)
void DOS_CharOutput(c8 chr);

// Output a '$'-terminated string (STROUT, 0x09)
void DOS_StringOutput(const c8* str);

// Play beep
inline void DOS_Beep() { DOS_CharOutput(CTRL_BEEP); }

// Clear console screen
inline void DOS_ClearScreen() { DOS_CharOutput(CTRL_CLS); }

// Carriage return
inline void DOS_Return() { DOS_StringOutput("\n\r$"); }

//.............................................................................
// Group: MSX-DOS 1 File Handling
#if (DOS_USE_FCB)

// Set the disk transfer address (SETDTA, 0x1A)
void DOS_SetTransferAddr(void* data);

// Get free space (free sectors) on a disk
u16 DOS_GetFreeSpace(u8 drive);

// Open a file described by an FCB (FOPEN, 0x0F)
u8 DOS_OpenFCB(DOS_FCB* stream);

// Get the size of an opened file
inline u32 DOS_GetSizeFCB(DOS_FCB* stream) { return stream->Size; }

// Close a file (FCLOSE, 0x10)
u8 DOS_CloseFCB(DOS_FCB* stream);

// Create a file (FMAKE, 0x16)
u8 DOS_CreateFCB(DOS_FCB* stream);

// Delete a file (FDEL, 0x13)
u8 DOS_DeleteFCB(DOS_FCB* stream);

// Sequential read (RDSEQ, 0x14)
u8 DOS_SequentialReadFCB(DOS_FCB* stream);

// Sequential write (WRSEQ, 0x15)
u8 DOS_SequentialWriteFCB(DOS_FCB* stream);

// Random block write (WRBLK, 0x26)
u8 DOS_RandomBlockWriteFCB(DOS_FCB* stream, u16 records);

// Random block read (RDBLK, 0x27) — returns number of records actually read
u16 DOS_RandomBlockReadFCB(DOS_FCB* stream, u16 records);

// Search first file matching wildcard (SFIRST, 0x11)
u8 DOS_FindFirstFileFCB(DOS_FCB* stream);

// Search next file matching wildcard (SNEXT, 0x12)
u8 DOS_FindNextFileFCB();

#endif // (DOS_USE_FCB)

//.............................................................................
// Group: MSX-DOS 1 Error Handling
#if (DOS_USE_ERROR_HANDLER)

// Install the DOS error handler in RAM
void DOS_InstallErrorHandler(callback cb, u8 size);

// Reset the last error code
inline void DOS_ResetLastError() { g_DOS_LastError = 0; }

#endif // (DOS_USE_ERROR_HANDLER)

//.............................................................................
// Group: MSX-DOS 1 Misc

// Get MSX-DOS version number (DOSVER, 0x6F) — returns kernel high part
u8 DOS_GetVersion(DOS_Version* ver);

// Select disk drive number (SELDSK, 0x0E) — returns number of drives
u8 DOS_SelectDrive(u8 drive);

// Select disk drive by letter
inline u8 DOS_SelectDriveLetter(c8 drive) { return DOS_SelectDrive('A' - drive); }

// Get bit field of available drives (LOGIN, 0x18)
u8 DOS_AvailableDrives() __FASTCALL;

// Get current drive number (CURDRV, 0x19)
u8 DOS_GetCurrentDrive();

// Get current drive letter
inline c8 DOS_GetCurrentDriveLetter() { return 'A' + DOS_GetCurrentDrive(); }

// Get disk information (ALLOC, 0x1B)
void DOS_GetDiskInfo(u8 drive, DOS_DiskInfo* info);

inline u16 DOS_GetFreeClusters(DOS_DiskInfo* info) { return info->FreeClusters; }
inline u32 DOS_GetFreeSectors(DOS_DiskInfo* info) { return (u32)info->FreeClusters * info->SectorPerCluster; }
inline u32 DOS_GetFreeBytes(DOS_DiskInfo* info) { return (u32)info->FreeClusters * info->SectorPerCluster * info->SectorSize; }
inline u16 DOS_GetTotalClusters(DOS_DiskInfo* info) { return info->TotalClusters; }
inline u32 DOS_GetTotalSectors(DOS_DiskInfo* info) { return (u32)info->TotalClusters * info->SectorPerCluster; }
inline u32 DOS_GetTotalBytes(DOS_DiskInfo* info) { return (u32)info->TotalClusters * info->SectorPerCluster * info->SectorSize; }

//=============================================================================
// MSX-DOS 2 FUNCTIONS
//=============================================================================

//.............................................................................
// Group: MSX-DOS 2 File Handling
#if (DOS_USE_HANDLE)

// Open a file handle (OPEN, 0x43) — returns new file handle
u8 DOS_OpenHandle(const c8* path, u8 mode);

// Create a file handle (CREATE, 0x44) — returns new file handle
u8 DOS_CreateHandle(const c8* path, u8 mode, u8 attr);

// Close a file handle (CLOSE, 0x45)
u8 DOS_CloseHandle(u8 file);

// Ensure a file handle (ENSURE, 0x46)
u8 DOS_EnsureHandle(u8 file);

// Duplicate a file handle (DUP, 0x47) — returns new handle
u8 DOS_DuplicateHandle(u8 file);

// Read from a file handle (READ, 0x48) — returns bytes read
u16 DOS_ReadHandle(u8 file, void* buffer, u16 size);

// Write to a file handle (WRITE, 0x49) — returns bytes written
u16 DOS_WriteHandle(u8 file, const void* buffer, u16 size);

// Move a file handle pointer (SEEK, 0x4A) — returns new pointer
u32 DOS_SeekHandle(u8 file, i32 offset, u8 mode) __CALLEE;

// Delete a file handle (HDELETE, 0x52)
u8 DOS_DeleteHandle(u8 file);

// Rename a file handle (HRENAME, 0x53)
u8 DOS_RenameHandle(u8 file, const c8* newPath);

// Move a file handle (HMOVE, 0x54)
u8 DOS_MoveHandle(u8 file, const c8* newPath);

// Set a file handle's attributes (HATTR set, 0x55)
u8 DOS_SetAttributeHandle(u8 file, u8 attr);

// Get a file handle's attributes (HATTR get, 0x55)
u8 DOS_GetAttributeHandle(u8 file);

#endif // (DOS_USE_HANDLE)

//.............................................................................
// Group: MSX-DOS 2 Core

// Get disk parameters (DPARM, 0x31)
u8 DOS_GetDiskParam(u8 drv, DOS_DiskParam* param);

// Terminate with error code (TERM, 0x62)
void DOS_Exit(u8 err);

// Explain error code (EXPLAIN, 0x66)
void DOS_Explain(u8 err, c8* str);

#if ((DOS_USE_VALIDATOR) || (DOS_USE_ERROR_HANDLER))
// Get last error code
inline u8 DOS_GetLastError() { return g_DOS_LastError; }
#endif

//.............................................................................
// Group: MSX-DOS 2 Utilities
#if (DOS_USE_UTILITIES)

// Find first entry (FFIRST, 0x40)
DOS_FIB* DOS_FindFirstEntry(const c8* filename, u8 attr);

// Find next entry (FNEXT, 0x41)
DOS_FIB* DOS_FindNextEntry();

// Get last file info
inline DOS_FIB* DOS_GetLastFileInfo() { return &g_DOS_LastFIB; }

inline u16 DOS_GetFileYear(const DOS_FIB* fib)   { return 1980 + (fib->Date >> 9); }
inline u8  DOS_GetFileMonth(const DOS_FIB* fib)  { return (fib->Date & 0x01FF) >> 5; }
inline u8  DOS_GetFileDay(const DOS_FIB* fib)    { return fib->Date & 0x1F; }
inline u8  DOS_GetFileHour(const DOS_FIB* fib)   { return fib->Time >> 11; }
inline u8  DOS_GetFileMinute(const DOS_FIB* fib) { return (fib->Time & 0x07FF) >> 5; }
inline u8  DOS_GetFileSecond(const DOS_FIB* fib) { return (fib->Time & 0x1F) << 1; }

// Delete file or subdirectory (DELETE, 0x4D)
u8 DOS_Delete(const c8* path);

// Rename file or subdirectory (RENAME, 0x4E)
u8 DOS_Rename(const c8* path, const c8* newPath);

// Move file or subdirectory (MOVE, 0x4F)
u8 DOS_Move(const c8* path, const c8* newPath);

// Set file attributes (ATTR set, 0x50)
u8 DOS_SetAttribute(const c8* path, u8 attr);

// Get file attributes (ATTR get, 0x50)
u8 DOS_GetAttribute(const c8* path);

// Get current directory of a drive (GETCD, 0x59)
u8 DOS_GetDirectory(u8 drive, const c8* path);

// Get current directory of the current drive
inline u8 DOS_GetCurrentDiskDirectory(const c8* path) { return DOS_GetDirectory(DOS_DRIVE_DEFAULT, path); }

// Change current directory (CHDIR, 0x5A)
u8 DOS_ChangeDirectory(const c8* path);

// Get current date and time (GDATE 0x2A + GTIME 0x2C)
const DOS_Time* DOS_GetTime();

#endif // (DOS_USE_UTILITIES)

//.............................................................................
// Group: BIOS
#if (DOS_USE_BIOSCALL)

// Read a value at an address in another slot (RDSLT, 0x000C)
u8 DOS_InterSlotRead(u8 slot, u16 addr);

// Write a value to an address in another slot (WRSLT, 0x0014)
void DOS_InterSlotWrite(u8 slot, u16 addr, u8 value);

// Inter-slot call (CALSLT, 0x001C)
void DOS_InterSlotCall(u8 slot, u16 addr);

#endif // (DOS_USE_BIOSCALL)

#endif // MSXMVL_DOS_H
