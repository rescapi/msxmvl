// msxmvl extension: raw disk sector I/O via the BIOS PHYDIO entry (0x0144).
//
// PHYDIO is the BIOS's physical-sector door to the disk interface ROM:
//
//   A  = drive (0 = A:)          HL = transfer address (must be RAM)
//   B  = number of sectors       Carry = 0 -> read, 1 -> write
//   C  = media descriptor        DE = first logical sector
//   -> Carry set on error, A = error code
//
// The arguments are staged through file-scope variables and handed to a NO-ARGUMENT
// naked routine on purpose. SDCC --sdcccall 1 passes trailing u8 args on the stack
// rather than in registers, and reading them from the wrong place is a silent,
// data-corrupting bug (exactly what was wrong in dos.c's DOS_OpenHandle). A no-arg
// asm routine has no calling convention left to get wrong.
#include "disk.h"

//=============================================================================
// GLOBALS
//=============================================================================

u8 g_Disk_Drive = 0;                    // A:
u8 g_Disk_Media = DISK_MEDIA_720K;

static u16 s_Sector;
static u8  s_Count;
static u8* s_Buffer;
static u8  s_Write;                     // 0 = read, 1 = write

//=============================================================================
// PHYDIO
//=============================================================================

// Perform the staged transfer. No arguments => no calling convention to get wrong.
// Returns 0 on success, else the disk ROM's error code.
static u8 Disk_Transfer(void) __naked
{
	__asm
		ld   hl, (_s_Buffer)    ; HL = transfer address
		ld   de, (_s_Sector)    ; DE = first sector
		ld   a, (_s_Count)
		ld   b, a               ; B = sector count
		ld   a, (_g_Disk_Media)
		ld   c, a               ; C = media descriptor

		ld   a, (_s_Write)
		or   a                  ; Z => read
		ld   a, (_g_Disk_Drive) ; A = drive  (LD does not touch the flags)
		jr   z, 1$
		scf                     ; carry SET  => write
		jr   2$
	1$:
		or   a                  ; carry CLEAR => read (OR A leaves A intact)
	2$:
		; PHYDIO clobbers freely and calls into the disk ROM's own slot; protect the
		; index registers SDCC expects us to preserve.
		push ix
		push iy
		call 0x0144             ; PHYDIO
		pop  iy
		pop  ix                 ; POP does not affect the carry from PHYDIO

		jr   c, 3$
		xor  a, a               ; success -> 0
		ret
	3$:
		ret                     ; A = disk ROM error code
	__endasm;
}

//=============================================================================
// FUNCTIONS
//=============================================================================

u8 Disk_ReadSectors(u16 sector, u8 count, void* buffer)
{
	s_Sector = sector;
	s_Count  = count;
	s_Buffer = (u8*)buffer;
	s_Write  = 0;
	return Disk_Transfer();
}

u8 Disk_WriteSectors(u16 sector, u8 count, const void* buffer)
{
	s_Sector = sector;
	s_Count  = count;
	s_Buffer = (u8*)buffer;
	s_Write  = 1;
	return Disk_Transfer();
}

//=============================================================================
// UNDER MSX-DOS: BDOS absolute sector read/write
//=============================================================================
// PHYDIO above is a BIOS entry, and under MSX-DOS page 0 is RAM -- the BIOS is not
// there to call. MSX-DOS's equivalent doors are BDOS 0x2F / 0x30, which are still
// RAW SECTOR access: no FAT, no directory, no files. Same idea as PHYDIO, reached
// the way a .COM has to reach it.
//
//   DE = first sector    H = sector count    L = drive (0 = A:)
//   buffer = the DTA (BDOS 0x1A)             -> A = 0 on success

static u8 DiskDOS_Transfer(void) __naked
{
	__asm
		ld   hl, (_s_Buffer)
		ex   de, hl
		ld   c, #0x1A           ; SETDTA -- the transfer address is a DOS global,
		call 0x0005             ; not an argument to the read/write call

		ld   de, (_s_Sector)    ; DE = first sector
		ld   a, (_s_Count)
		ld   h, a               ; H  = sector count
		ld   a, (_g_Disk_Drive)
		ld   l, a               ; L  = drive

		ld   a, (_s_Write)
		or   a
		ld   c, #0x2F           ; RDABS (LD does not touch the flags)
		jr   z, 1$
		ld   c, #0x30           ; WRABS
	1$:
		call 0x0005
		ret                     ; A = error code (0 = success)
	__endasm;
}

u8 DiskDOS_ReadSectors(u16 sector, u8 count, void* buffer)
{
	s_Sector = sector;
	s_Count  = count;
	s_Buffer = (u8*)buffer;
	s_Write  = 0;
	return DiskDOS_Transfer();
}

u8 DiskDOS_WriteSectors(u16 sector, u8 count, const void* buffer)
{
	s_Sector = sector;
	s_Count  = count;
	s_Buffer = (u8*)buffer;
	s_Write  = 1;
	return DiskDOS_Transfer();
}
