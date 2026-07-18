// msxmvl EXTENSION module: sram -- battery-backed game saves, block level.
//
// Real MSX hardware has two battery-backed answers to "save without a disk",
// and they share NO register, address range or discovery mechanism -- so this
// API is honest only at the BLOCK level (detect / size / read / write / verify)
// with the mapping mechanics per compile-time backend, exactly the memseg
// backend pattern. Hardware ground truth: docs/SRAM.md (openMSX device
// sources = emulation contract).
//
// Backend (compile-time):
//   SRAM_BACKEND_PAC (default) -- a PAC / FM-PAC cartridge (Panasoft SW-M001 /
//       SW-M004): 8 KB SRAM at 0x4000-0x5FFD in the PAC's own slot, unlocked by
//       writing 'M' to 0x5FFE and 'i' to 0x5FFF (anything else relocks). Access
//       is per-byte interslot (BIOS RDSLT/WRSLT), so it works from any program
//       with zero link-time knowledge. Every block call relocks on the way out:
//       a crash can never leave the SRAM writable.
//   SRAM_BACKEND_MAPPER -- an SRAM chip wired into your own ASCII8/ASCII16
//       MegaROM (the cartridge bankpack builds). The SRAM is an extra segment
//       behind the normal bank-select register: each block call brackets one
//       fast local copy with di / save the window segment (from the SHADOW
//       byte -- the registers are write-only) / select SRAM / copy / restore /
//       ei. No open/close is exposed, so a stale-mapping bug class can't exist.
//       Needs the SRAM_SELECT/SRAM_SIZE/SRAM_SHADOW/SRAM_BANKREG constants that
//       bankpack emits into far.h from the manifest's `SRAM <kb>` line.
#ifndef MSXMVL_SRAM_H
#define MSXMVL_SRAM_H

#include "types.h"

#if defined(SRAM_BACKEND_PAC) && defined(SRAM_BACKEND_MAPPER)
#error "pick ONE sram backend: SRAM_BACKEND_PAC or SRAM_BACKEND_MAPPER"
#endif
#if !defined(SRAM_BACKEND_PAC) && !defined(SRAM_BACKEND_MAPPER)
#define SRAM_BACKEND_PAC
#endif

// Is a battery-backed SRAM actually there?
//   PAC:    scan every (sub)slot with the lock/unlock protocol; remember the
//           first hit in g_Sram_Slot. Safe on RAM, ROM and empty slots (every
//           probed byte is restored).
//   MAPPER: map the SRAM segment into the window, RAM-test one byte, restore.
// Call it once at startup, before the other calls.
bool Sram_Detect(void);

// Usable bytes: PAC 8190 (0x4000-0x5FFD; the last two bytes are the lock
// registers); MAPPER: the manifest's SRAM size (2048/8192/32768).
u16  Sram_Size(void);

// Copy n bytes SRAM[ofs..] -> dst / src -> SRAM[ofs..]. The open + copy +
// close (relock / segment restore) happens INSIDE the call. A range that does
// not fit Sram_Size() is rejected whole -- no partial saves.
void Sram_Read (u16 ofs, void* dst, u16 n);
void Sram_Write(u16 ofs, const void* src, u16 n);

// Read-back compare: TRUE if SRAM[ofs..ofs+n) == src[0..n). Call it after
// Sram_Write -- a dead battery fails here, not silently at the next power-on.
// FALSE for an out-of-range span or when no SRAM was detected.
bool Sram_Verify(u16 ofs, const void* src, u16 n);

#ifdef SRAM_BACKEND_PAC

#define SRAM_PAC_BYTES  8190u   // 0x4000-0x5FFD in the PAC's slot, page 1

#ifndef SLOT_NOTFOUND
#define SLOT_NOTFOUND   0xFF
#endif

// Slot ID (ExxxSSPP) of the PAC found by Sram_Detect, or SLOT_NOTFOUND.
extern u8 g_Sram_Slot;

// ---- shared PAC core (lib/gen/pac.c, the MSXgl-compatible surface, is a ----
// ---- thin layer over these; they are not part of the block API above)   ----
// addr is the CPU address in the PAC's slot: 0x4000..0x5FFD.
bool Sram_PacTest(u8 slot);                    // the detect protocol, one slot
u8   Sram_PacScan(u8* slots, u8 max);          // fill up to max slot IDs, return count
void Sram_PacActivate(u8 slot, u8 enable);     // raw unlock/relock (PAC_Activate)
void Sram_PacRead (u8 slot, u16 addr, u8* dst, u16 n);        // unlock+copy+relock
void Sram_PacWrite(u8 slot, u16 addr, const u8* src, u16 n);  // unlock+copy+relock
void Sram_PacFill (u8 slot, u16 addr, u8 val, u16 n);         // unlock+fill+relock
bool Sram_PacIsAll(u8 slot, u16 addr, u8 val, u16 n);         // every byte == val?

#endif // SRAM_BACKEND_PAC

#endif // MSXMVL_SRAM_H
