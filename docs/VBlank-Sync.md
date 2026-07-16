# VBlank Sync (`vsync`)

One function: block until the next vertical blank. This is how you pace a frame loop to 50/60 Hz
and how you time a page flip so it happens off-screen (no tearing). Link `vsync.c`, include
`vsync.h`.

---

## `VDP_WaitVBlank`

```c
void VDP_WaitVBlank(void);
```

Polls the VDP status flag S#0 bit 7 (the vertical-blank flag) and returns the moment it sets.
Two details make it safe to drop into any code:

1. **Interrupt-state preserving.** It masks interrupts internally (so an ISR can't corrupt the
   VDP address latch mid-poll), but it restores the caller's exact interrupt-enable state on
   return — enter with interrupts on, leave with them on; enter with them off, leave them off.
2. **Clears stale flags first.** It reads the status register once before polling, so it always
   waits for a *fresh* vblank rather than returning instantly on an already-pending one.

**Example — one call per frame to pace the game loop.**

```c
// gameloop.c — pace the game to the display's refresh, tear-free.
#include "vsync.h"

// Wait for the start of the next frame's vertical blank.
void wait_for_frame(void)
{
	VDP_WaitVBlank();
}
```

`wait_for_frame()` blocks until the next vblank and returns without hanging, leaving the
caller's interrupts exactly as it found them — so a loop that runs music under interrupts can
call it every frame and stay enabled. *(tested: `vsync_01_wait.c`)*

> **Why not use the interrupt instead?** You can, but a busy `WaitVBlank` is simpler for a
> straight-line frame loop and needs no ISR setup. For background music or timers, enable the
> VDP interrupt and do your per-frame work in the handler; `WaitVBlank` and interrupts coexist
> because the wait restores IFF.

### In a frame loop

```c
for (;;) {
	// ... update + draw into the hidden page ...
	VDP_WaitVBlank();     // pace to one field
	Display_Flip();       // flip during blanking -> no tearing
}
```

## See also

- [Double Buffering](Double-Buffering.md) — the flip this synchronizes.
