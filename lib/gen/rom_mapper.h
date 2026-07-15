// msxmvl clean-room reimplementation of MSXgl "rom_mapper" module.
// Public names + signatures match MSXgl exactly (drop-in replacement).
// Derived from MSXgl engine/src/rom_mapper.h (interface only) +
// standard MSX ROM-mapper knowledge (ASCII-8 / ASCII-16 / Konami bank switch).
//
// In MSXgl these are `inline` functions living in rom_mapper.h, selected at
// build time by the ROM_MAPPER configuration macro. msxmvl exposes them as
// REAL, linkable functions with the exact same names/signatures so a single
// difftest driver can link against real MSXgl OR msxmvl.
//
// MAPPER CHOICE: this build implements the ASCII-8 mapper (ROM_ASCII8), the
// most common 8 KB ROM mapper. It has:
//   MAPPER_BANKS   = 4          (4 banks of 8 KB spanning 4000h-BFFFh)
//   8-bit segments (u8 g_Bank0Segment[4])
//   bank switch write addresses:
//     bank 0 -> 6000h  (maps 4000h-5FFFh)
//     bank 1 -> 6800h  (maps 6000h-7FFFh)
//     bank 2 -> 7000h  (maps 8000h-9FFFh)
//     bank 3 -> 7800h  (maps A000h-BFFFh)
// Selecting a segment = writing the segment number to the bank's switch
// address (SET) while keeping a RAM shadow copy in g_Bank0Segment (GET reads
// the shadow, since the switch addresses are write-only on real hardware).
#ifndef MSXMVL_ROM_MAPPER_H
#define MSXMVL_ROM_MAPPER_H

#include "types.h"

//=============================================================================
// SDCC attribute macros (mirror MSXgl core.h)
//=============================================================================
#ifndef __PRESERVES
#define __PRESERVES		__preserves_regs
#endif

//=============================================================================
// DEFINES (ASCII-8 mapper)
//=============================================================================

#define MAPPER_BANKS		4
#define ROM_SEGMENT_SIZE	8

#define ADDR_BANK_0			0x6000 // 4000h - 5FFFh
#define ADDR_BANK_1			0x6800 // 6000h - 7FFFh
#define ADDR_BANK_2			0x7000 // 8000h - 9FFFh
#define ADDR_BANK_3			0x7800 // A000h - BFFFh

//=============================================================================
// GLOBALS
//=============================================================================

// Segment value backup (RAM shadow of the write-only bank switch registers).
extern u8 g_Bank0Segment[MAPPER_BANKS];

//=============================================================================
// FUNCTIONS
//=============================================================================
// The header declares no __PRESERVES() contract on any of these (they are
// `inline` in MSXgl), so they follow the standard SDCC calling convention.

// Function: SET_BANK_SEGMENT
// Set the current segment of the given bank.
//
// Parameters:
//   b - Bank number to set (0-3 for ASCII-8)
//   s - Segment to select in this bank (8-bit)
void SET_BANK_SEGMENT(u8 b, u8 s);

// Function: SET_BANK_SEGMENT_LOW
// Set the low byte of the current segment of the given bank.
// For 8-bit mappers this is identical to SET_BANK_SEGMENT.
void SET_BANK_SEGMENT_LOW(u8 b, u8 s);

// Function: SET_BANK_SEGMENT_HIGH
// Set the high byte of the current segment of the given bank.
// No-op for 8-bit mappers (no high byte).
void SET_BANK_SEGMENT_HIGH(u8 b, u8 s);

// Function: GET_BANK_SEGMENT
// Get the current segment of the given bank.
//
// Return:
//   Segment selected in this bank (8-bit)
u8 GET_BANK_SEGMENT(u8 b);

// Function: GET_BANK_SEGMENT_LOW
// Get the low byte of the current segment of the given bank.
// For 8-bit mappers this is identical to GET_BANK_SEGMENT.
u8 GET_BANK_SEGMENT_LOW(u8 b);

// Function: GET_BANK_SEGMENT_HIGH
// Get the high byte of the current segment of the given bank.
// Always 0 for 8-bit mappers.
u8 GET_BANK_SEGMENT_HIGH(u8 b);

//=============================================================================
// YAMANOOTO flashcart control (config/enable/offset registers)
//=============================================================================
// The Yamanooto cartridge exposes three memory-mapped control registers in the
// 0x7FFD-0x7FFF window (documented hardware protocol). These configure the
// cartridge's extended features independently of the ROM banking above.
#define YAMANOOTO_CFGR			0x7FFD	// configuration register
#define YAMANOOTO_OFFR			0x7FFE	// segment offset register
#define YAMANOOTO_ENAR			0x7FFF	// enable register
#define YAMANOOTO_ENAR_REGEN	0x01	// register-access enable bit

// Enable or disable the Yamanooto extended-register access.
inline void YAMANOOTO_ENABLE(bool enable) { *(volatile u8*)YAMANOOTO_ENAR = (u8)(enable ? YAMANOOTO_ENAR_REGEN : 0); }
// Write the enable register (ENAR, 0x7FFF).
inline void YAMANOOTO_SET_ENAR(u8 value) { *(volatile u8*)YAMANOOTO_ENAR = value; }
// Write the offset register (OFFR, 0x7FFE).
inline void YAMANOOTO_SET_OFFR(u8 value) { *(volatile u8*)YAMANOOTO_OFFR = value; }
// Write the configuration register (CFGR, 0x7FFD).
inline void YAMANOOTO_SET_CFGR(u8 value) { *(volatile u8*)YAMANOOTO_CFGR = value; }

#endif // MSXMVL_ROM_MAPPER_H
