// msxmvl extension module: vertical-blank synchronization. NOT part of the MSXgl API.
// Self-contained (polls the VDP status port directly; no interrupt setup required).
#ifndef MSXMVL_EXT_VSYNC_H
#define MSXMVL_EXT_VSYNC_H

#include "types.h"

// Busy-wait until the start of the next VDP vertical blank, by polling S#0 bit 7.
//
// Interrupt-safe. The catch this encapsulates (learned the hard way in the g3d_cube demo):
// if interrupts are enabled, the C-BIOS interrupt handler reads S#0 on every vblank and
// clears the flag before a naive poll can see it — so the poll misses vblanks and stalls.
// This routine masks interrupts around the poll, then restores the caller's prior
// interrupt-enable state on return. It also clears any already-pending flag first, so each
// call blocks for a *fresh* vblank (one full field, ~one frame).
//
// Pair with the display extension for a tear-free page flip:
//     VDP_WaitVBlank(); Display_Flip();
void VDP_WaitVBlank(void);

#endif // MSXMVL_EXT_VSYNC_H
