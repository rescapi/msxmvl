// msxmvl EXTENSION module: isr -- take over the interrupt with IM 2.
//
// The normal MSX interrupt path runs the WHOLE BIOS ISR on every 50/60 Hz tick
// (register saves, status reads, keyboard scan, hooks) before your H.TIMI hook
// sees anything. IM 2 bypasses it completely: the Z80 vectors through a table
// in RAM straight to this module's dispatcher, which saves registers, reads the
// VDP status (acknowledging the interrupt), and calls YOUR plain C handler.
// Works on any program type -- 16/32/48 KB ROMs and banked MegaROMs alike --
// because IM 2 never touches page 0.
//
// The price: while installed, the BIOS interrupt work does not happen. No
// JIFFY tick, no BIOS keyboard scan, no H.TIMI hooks -- your handler IS the
// interrupt. ISR_UninstallIM2 returns to IM 1 (the BIOS on 16/32 KB carts;
// crt0_rom48's RAM vector on 48 KB ones).
//
// RAM use (fixed, documented): the 257-byte IM 2 vector table at 0xE300-0xE400
// (every byte 0xE4) and a 3-byte jump thunk at 0xE4E4. Override the pages with
// -DISR_IM2_TABLE_PAGE=0x.. -DISR_IM2_THUNK_PAGE=0x.. if your program owns
// that RAM (thunk page must equal the table's fill byte).
//
// Handler contract: an ordinary void(void) C function. Registers are saved and
// restored around it (main set + IX/IY; the alternate set is NOT touched --
// SDCC-generated code never uses it). Keep it short: it runs inside the
// interrupt. Read the VDP status byte captured at entry with ISR_Status()
// (bit 7 = this was a VBLANK interrupt).
#ifndef MSXMVL_EXT_ISR_H
#define MSXMVL_EXT_ISR_H

#include "types.h"

#ifndef ISR_IM2_TABLE_PAGE
#define ISR_IM2_TABLE_PAGE  0xE3   // vector table at 0xE300-0xE400
#endif
#ifndef ISR_IM2_THUNK_PAGE
#define ISR_IM2_THUNK_PAGE  0xE4   // table fill byte; thunk at 0xE4E4
#endif

typedef void (*ISR_Handler)(void);

// Function: ISR_InstallIM2
// Build the vector table + thunk in RAM, point them at your handler, and
// switch to IM 2. From the next interrupt on, the BIOS ISR is out of the loop.
void ISR_InstallIM2(ISR_Handler handler);

// Function: ISR_UninstallIM2
// Back to IM 1: the page-0 vector owner (BIOS, or crt0_rom48) handles
// interrupts again from the next tick.
void ISR_UninstallIM2(void);

// Function: ISR_Status
// The VDP S#0 byte read (and thereby acknowledged) at interrupt entry, for the
// current/most recent handler run. Bit 7 set = VBLANK interrupt.
u8 ISR_Status(void);

#endif // MSXMVL_EXT_ISR_H
