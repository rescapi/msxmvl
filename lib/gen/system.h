// msxmvl clean-room reimplementation of MSXgl "system" module.
// Drop-in: exposes MSXgl's exact public names + signatures so one difftest
// driver can link against real MSXgl OR msxmvl.
//
// The upstream header pulls in core.h / system_port.h / asm.h / bios_var.h /
// bios_mainrom.h and branches on TARGET_TYPE. For a self-contained build we
// declare only the handful of symbols the public inline bodies touch, and pick
// the default "read from Main-ROM variables" path for the Information group.
#ifndef MSXMVL_SYSTEM_H
#define MSXMVL_SYSTEM_H

#include "types.h"

// SDCC single-argument-in-register calling convention (upstream: core.h).
#ifndef __FASTCALL
#define __FASTCALL __z88dk_fastcall
#endif

//=============================================================================
// DEFINES
//=============================================================================

//-----------------------------------------------------------------------------
// Slot ID  ->  ExxxSSPP
//   PP = primary slot (00-11), SS = secondary slot (00-11), E = expanded flag

// Primary slots
#define SLOT_0			(0x00)
#define SLOT_1			(0x01)
#define SLOT_2			(0x02)
#define SLOT_3			(0x03)

#define SLOT_EXP		(1 << 7)
// Expanded slots		Primary   Secondary       Expand
#define SLOT_0_0		(SLOT_0 | (SLOT_0 << 2) | SLOT_EXP)
#define SLOT_0_1		(SLOT_0 | (SLOT_1 << 2) | SLOT_EXP)
#define SLOT_0_2		(SLOT_0 | (SLOT_2 << 2) | SLOT_EXP)
#define SLOT_0_3		(SLOT_0 | (SLOT_3 << 2) | SLOT_EXP)
#define SLOT_1_0		(SLOT_1 | (SLOT_0 << 2) | SLOT_EXP)
#define SLOT_1_1		(SLOT_1 | (SLOT_1 << 2) | SLOT_EXP)
#define SLOT_1_2		(SLOT_1 | (SLOT_2 << 2) | SLOT_EXP)
#define SLOT_1_3		(SLOT_1 | (SLOT_3 << 2) | SLOT_EXP)
#define SLOT_2_0		(SLOT_2 | (SLOT_0 << 2) | SLOT_EXP)
#define SLOT_2_1		(SLOT_2 | (SLOT_1 << 2) | SLOT_EXP)
#define SLOT_2_2		(SLOT_2 | (SLOT_2 << 2) | SLOT_EXP)
#define SLOT_2_3		(SLOT_2 | (SLOT_3 << 2) | SLOT_EXP)
#define SLOT_3_0		(SLOT_3 | (SLOT_0 << 2) | SLOT_EXP)
#define SLOT_3_1		(SLOT_3 | (SLOT_1 << 2) | SLOT_EXP)
#define SLOT_3_2		(SLOT_3 | (SLOT_2 << 2) | SLOT_EXP)
#define SLOT_3_3		(SLOT_3 | (SLOT_3 << 2) | SLOT_EXP)

// Slot macros
#define SLOT(p)			(0x03 & (p))
#define SLOTEX(p, s)	((0x03 & (p)) | ((0x03 & (s)) << 2) | SLOT_EXP)

#define IS_SLOT_EXP(s)	((s) & SLOT_EXP)
#define SLOT_PRIM(s)	((s) & 0x03)
#define SLOT_SEC(s)		(((s) >> 2 ) & 0x03)

// Special slot ID values
#define SLOT_NOTFOUND 	0xFF
#define SLOT_INVALID 	0xFF

// Function callback to make iterations on slots
typedef bool (*CheckSlotCallback)(u8);

//-----------------------------------------------------------------------------
// System frequency
#define SYS_FREQ_50HZ		0x80 // 50 Hz frequency
#define SYS_FREQ_60HZ		0x00 // 60 Hz frequency

//-----------------------------------------------------------------------------
// System globals referenced by the public inline bodies.

// EXPTBL (0xFCC1..): bit7 of each entry flags whether that primary slot is
// expanded. g_EXPTBL[0] also holds the Main-ROM's slot ID.
extern const u8 __at(0xFCC1) g_EXPTBL[4];

// Main-ROM version bytes (BASRVN) and MSX version number (MSXVER).
extern const u8 __at(0x002B) g_BASRVN[2];
extern const u8 __at(0x002D) g_MSXVER;

// PPI port C (0xAA): general I/O incl. the keyboard-click bit (bit7).
__sfr __at(0xAA) g_PortAccessKeyboard;

// CRT0 linker markers (program extents).
extern u16 g_FirstAddr;  // First address of the program
extern u16 g_HeaderAddr; // Address of the header
extern u16 g_LastAddr;   // Last address of the program

//=============================================================================
// FUNCTIONS
//=============================================================================

//-----------------------------------------------------------------------------
// Group: Peek & poke

// Set a 8-bit value at the given address.
inline void Poke(u16 addr, u8 val) { *(u8*)addr = val; }

// Set a 16-bit value at the given address.
inline void Poke16(u16 addr, u16 val) { *(u16*)addr = val; }

// Get the 8-bit value at the given address.
inline u8 Peek(u16 addr) { return *(u8*)addr; }

// Get the 16-bit value at the given address.
inline u16 Peek16(u16 addr) { return *(u16*)addr; }

//-----------------------------------------------------------------------------
// Group: Low level

// Enable interruption.
inline void EnableInterrupt() { __asm__("ei"); }

// Disable interruption.
inline void DisableInterrupt() { __asm__("di"); }

// Pause the CPU until a new interruption occured.
inline void Halt() { __asm__("halt"); }

//-----------------------------------------------------------------------------
// Group: Information

// Get BIOS information (frequency, keyboard type, etc.).
inline u8 Sys_GetBIOSInfo() { return g_BASRVN[0]; }

// Get the MSX version (0: MSX1, 1: MSX2, 2: MSX2+, 3: MSX turbo R).
inline u8 Sys_GetMSXVersion() { return g_MSXVER; }

// Get the system frequency (from BIOS): SYS_FREQ_50HZ or SYS_FREQ_60HZ.
inline u8 Sys_GetFrequency() { return Sys_GetBIOSInfo() & 0x80; }

// TRUE if the BIOS is 60 Hz.
inline bool Sys_Is60Hz() { return Sys_GetFrequency() == SYS_FREQ_60HZ; }

// TRUE if the BIOS is 50 Hz.
inline bool Sys_Is50Hz() { return Sys_GetFrequency() == SYS_FREQ_50HZ; }

//-----------------------------------------------------------------------------
// Group: Call

// Direct call a routine at a given address.
inline void Call(u16 addr) { ((void(*)(void))addr)(); }

// Direct call a routine; return register A.
inline u8 CallToA(u16 addr) { return ((u8(*)(void))addr)(); }

// Direct call a routine with a 8-bit parameter in register A.
inline void CallA(u16 addr, u8 a) { ((void(*)(u8))addr)(a); }

// Direct call a routine with a 8-bit parameter in register A; return A.
inline u8 CallAToA(u16 addr, u8 a) { return ((u8(*)(u8))addr)(a); }

typedef void (*call_l_t)(u8) __FASTCALL;

// Direct call a routine with a 8-bit parameter in register L.
inline void CallL(u16 addr, u8 l) { ((call_l_t)addr)(l); }

// Upstream types this as returning void; CallLToA nonetheless yields register A
// (the __FASTCALL return). We type it u8 so the returned value propagates and
// the public 'u8 CallLToA(...)' contract holds.
typedef u8 (*call_l_a_t)(u8) __FASTCALL;

// Direct call a routine with a 8-bit parameter in register L; return A.
inline u8 CallLToA(u16 addr, u8 l) { return ((call_l_a_t)addr)(l); }

// Direct call a routine with a 16-bit parameter in register HL.
inline void CallHL(u16 addr, u16 hl) { ((void(*)(u16))addr)(hl); }

// Direct call a routine with a 16-bit parameter in register HL; return A.
inline u8 CallHLToA(u16 addr, u16 hl) { return ((u8(*)(u16))addr)(hl); }

// Direct call a driver's main function with a 8-bit value in register A.
inline u8 CallDriver(u16 addr, u8 val) { return ((u8(*)(u8))addr)(val); }

//-----------------------------------------------------------------------------
// Group: Slot

// Get the slot ID of a given page.
u8 Sys_GetPageSlot(u8 page);

// Select a slot in a given page.
void Sys_SetPageSlot(u8 page, u8 slotId);

// Check if slot is expanded.
inline bool Sys_IsSlotExpanded(u8 slot) { return g_EXPTBL[slot] & SLOT_EXP; }

// Select a given slot in page 0.
void Sys_SetPage0Slot(u8 slotId);

// Check expended flag from slot ID.
inline bool Sys_SlotIsExpended(u8 slotId) { return IS_SLOT_EXP(slotId); }

// Get primary slot from slot ID.
inline u8 Sys_SlotGetPrimary(u8 slotId) { return SLOT_PRIM(slotId); }

// Get secondary slot from slot ID.
inline u8 Sys_SlotGetSecondary(u8 slotId) { return SLOT_SEC(slotId); }

// Check all slots with a given callback function; return the matching slot ID.
u8 Sys_CheckSlot(CheckSlotCallback cb);

//-----------------------------------------------------------------------------
// Group: Address

// Get first address of program binary.
inline u16 Sys_GetFirstAddr() { return (u16)&g_FirstAddr; }

// Get address of program header (if any).
inline u16 Sys_GetHeaderAddr() { return (u16)&g_HeaderAddr; }

// Get last address of program binary.
inline u16 Sys_GetLastAddr() { return (u16)&g_LastAddr; }

//-----------------------------------------------------------------------------
// Group: Misc

// Play the click sound.
inline void Sys_PlayClickSound() { g_PortAccessKeyboard |= 0x80; }

// Stop the click sound.
inline void Sys_StopClickSound() { g_PortAccessKeyboard &= 0x7F; }

#endif // MSXMVL_SYSTEM_H
