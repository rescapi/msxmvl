# Segment Windowing (`memseg`)

The low-level layer for the **MSX2 RAM memory mapper** — the hardware that lets a machine hold
more than 64 KB of RAM. Link `memseg.c`, include `memseg.h`. Most code uses the higher-level
[Far Pointers](Far-Pointers.md) API on top of this; reach for `memseg` directly when you want to
map a segment and run your own code against the window.

> **MSX2 only — and it will not tell you.** An MSX1 has no RAM memory mapper. `memseg` will still
> *appear* to work there: `MemSeg_SetWindow` writes to a port that isn't answered, and
> `MemSeg_GetWindow` then reads the **software shadow**, which faithfully reports the segment you
> asked for. The shadow is not a presence check — it's a cache of your own intent. Only
> [`farmem`](Far-Pointers.md), which actually reads through the window, notices that nothing is
> mapped. If you need a lot of code (not data) on an MSX1, use
> [Code Banking](Code-Banking.md) instead — MegaROM banking is cartridge hardware and works there.

## Why a shadow is mandatory

The Z80 sees 64 KB as four 16 KB pages. The RAM mapper swaps 16 KB **segments** into a page by
writing the segment number to an I/O port (`0xFE` for page 2). Two hard facts shape the API:

- **The port is write-only.** There is no way to ask the hardware "which segment is mapped
  right now?" So `memseg` keeps a **RAM shadow** of the current segment, updated on every write.
  That shadow is the only way to save-and-restore a temporary switch.
- **You must window a page you're *not* running from.** `memseg` uses **page 2** (`0x8000`–
  `0xBFFF`): your code is in page 1, your stack in page 3, so page 2 is the safe one to repurpose.

On C-BIOS/openMSX the mapper is 512 KB = **32 segments** (numbers `0..31`).

---

## `MemSeg_Init` / `MemSeg_SetWindow` / `MemSeg_GetWindow`

```c
void   MemSeg_Init(MemSeg home);       // map `home` into the window and sync the shadow
void   MemSeg_SetWindow(MemSeg seg);   // map `seg`, update the shadow (no save)
MemSeg MemSeg_GetWindow(void);         // read the shadow: what's mapped now
```

Call `MemSeg_Init` once at startup with a "home" segment (one not used by your stack/data).
Because the boot segment can't be read back, `Init` *imposes* a known mapping rather than
discovering one — from then on the shadow always matches hardware.

**Example — set and read back the window.**

```c
#include "memseg.h"
volatile u8 __at(0xE000) R[4];

void main(void)
{
	MemSeg_Init(16);                    // window = segment 16
	R[1] = MemSeg_GetWindow();          // 16

	MemSeg_SetWindow(20);               // now segment 20 is in the window
	R[2] = MemSeg_GetWindow();          // 20

	R[0] = (R[1] == 16 && R[2] == 20) ? 0xA5 : 0x00;
	for (;;) {}
}
```

Runs to `R[] = a5 10 14` (`0x10=16`, `0x14=20`). *(tested: `memseg_01_setget.c`)*

---

## `MemSeg_Window` / `MemSeg_Restore`

```c
MemSeg MemSeg_Window(MemSeg seg);   // map `seg`, RETURN the segment that was there
void   MemSeg_Restore(MemSeg prev); // put `prev` back
```

The save-and-restore pair. `MemSeg_Window` maps a new segment and hands you the previous one (from
the shadow) so you can restore it — the correct pattern for a *temporary* switch, and what makes
nested access safe (each level restores what it found).

**Example — map a segment, use it, restore.**

```c
#include "memseg.h"
volatile u8 __at(0xE000) R[4];

void main(void)
{
	MemSeg_Init(16);                        // home = 16
	*(volatile u8*)0x8000 = 0xAA;           // write into segment 16

	MemSeg prev = MemSeg_Window(17);        // map 17, prev == 16
	*(volatile u8*)0x8000 = 0xBB;           // write into segment 17
	MemSeg_Restore(prev);                   // back to 16

	R[1] = prev;                            // 16
	R[2] = *(volatile u8*)0x8000;           // 0xAA — segment 16 kept its value
	R[0] = (prev == 16 && R[2] == 0xAA) ? 0xA5 : 0x00;
	for (;;) {}
}
```

Runs to `R[] = a5 10 aa` — writing `0xBB` through the window while segment 17 was mapped did not
disturb segment 16's `0xAA`. The two segments are physically distinct RAM. *(tested:
`memseg_02_window.c`)*

> **Window base is `0x8000`.** Anything you read/write at `0x8000..0xBFFF` after a `Window` call
> hits the mapped segment. The [Far Pointers](Far-Pointers.md) API wraps this so you don't handle
> raw window addresses yourself.

## See also

- [Far Pointers](Far-Pointers.md) — `FarPtr`, block copy, and the scoped-borrow `Far_With`, all
  built on these primitives.
