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

**Example — confirm it returns and preserves the interrupt state.**

```c
#include "vsync.h"
volatile u8 __at(0xE000) R[2];

void main(void)
{
	__asm ei __endasm;
	VDP_WaitVBlank();              // waits one field, then returns (no hang)

	__asm
		ld   a, i                 ; P/V = IFF2 (interrupt-enable state)
		jp   pe, 1$               ; P/V=1 -> interrupts still enabled
		xor  a
		jr   2$
	1$:
		ld   a, #0xA5
	2$:
		ld   (_R + 0), a          ; R[0] = 0xA5 iff interrupts preserved
	__endasm;
	for (;;) {}
}
```

Runs to `R[] = a5` — the call returned (no hang) and interrupts are still enabled.
*(tested: `vsync_01_wait.c`)*

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
