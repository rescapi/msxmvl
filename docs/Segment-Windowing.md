# Segment Windowing (`memseg`)

This is the machinery underneath [Far Pointers](Far-Pointers.md) — the raw way to swap 16 KB
**segments** of mapper RAM in and out of the Z80's view. Most code should use the friendlier
[Far Pointers](Far-Pointers.md) API and never come here. Reach for `memseg` directly only when you
want to map a segment yourself and run your own tight loop against it.

> Link `memseg.c`, include `memseg.h`.

> **Reading the examples.** Each snippet below is a complete, auto-tested program. The
> `volatile u8 __at(0xE000) R[...]` buffer and the closing `for (;;) {}` are just test scaffolding
> (the harness reads `R[]` back; `R[0] = 0xA5` means "passed"); in your own code you keep only the
> `MemSeg_*` calls. ([full explanation](Getting-Started.md))

## What "windowing" means

The Z80 addresses 64 KB, split into four 16 KB **pages**. A RAM mapper has many more 16 KB
**segments** than that (512 KB = 32 segments), and it lets you choose which segment appears in a
page — like sliding one interchangeable cartridge at a time into a slot you can see through. That
visible slot is the **window**. `memseg` uses **page 2** (`0x8000`–`0xBFFF`) for it: your program
runs in page 1 and your stack lives in page 3, so page 2 is the safe one to repurpose.

Two hardware facts shape the whole API:

- **The mapper register is write-only.** You can tell the hardware "show segment 20 now", but you
  can't ask it "which segment is showing?". So `memseg` keeps a **software note** (a "shadow") of
  the current segment, updated every time you change it. That shadow is what makes a
  save-and-restore possible.
- **Init imposes a mapping, it doesn't discover one.** Because the boot segment can't be read
  back, `MemSeg_Init` *sets* a known "home" segment rather than detecting whatever was there. From
  that point on the shadow always matches the hardware.

> **MSX2 only — and it fails quietly.** An MSX1 has no RAM mapper. On MSX1, `MemSeg_SetWindow`
> writes to a port nothing answers, and `MemSeg_GetWindow` then returns the **shadow** — which
> faithfully reports the segment you *asked* for, even though nothing was actually mapped. The
> shadow is a record of your intent, not a presence check. Only [`farmem`](Far-Pointers.md), which
> reads back through the window, can tell that nothing is there. If you need more *code* (not data)
> on an MSX1, use [Code Banking](Code-Banking.md) instead — MegaROM banking is cartridge hardware
> and works on every MSX.

---

## Pick a segment — `MemSeg_Init` / `MemSeg_SetWindow` / `MemSeg_GetWindow`

```c
void   MemSeg_Init(MemSeg home);       // start up: map `home` and record it in the shadow
void   MemSeg_SetWindow(MemSeg seg);   // show `seg` now (updates the shadow)
MemSeg MemSeg_GetWindow(void);         // which segment is showing? (reads the shadow)
```

Call `MemSeg_Init` once at startup with a "home" segment. After that, `SetWindow` flips the visible
segment and `GetWindow` tells you which one it is.

```c
// SEGMENT WINDOWING — the raw mechanism behind "banking".
//
// The Z80 can only address 64 KB at once, but an MSX memory mapper holds much more
// (512 KB and up), split into 16 KB "segments" (think: interchangeable cartridges).
// You choose which segment is visible through a fixed "window" at address 0x8000.
// Only ONE segment shows through the window at a time; swapping is instant.
//
// Snag: the hardware register that selects the segment is WRITE-ONLY — you can set it
// but not read it back. So the library keeps a software note of the current segment.
// MemSeg_Init picks a starting ("home") segment; MemSeg_SetWindow swaps another one in;
// MemSeg_GetWindow tells you which is showing right now.
//
// Here: a game keeps its title screen in one segment and level 1 in another, and flips
// the window between them.
#include "memseg.h"
volatile u8 __at(0xE000) R[4];

#define SEG_TITLE   16      // 16 KB segment holding the title screen
#define SEG_LEVEL1  20      // ...and the one holding level 1

void main(void)
{
	MemSeg_Init(SEG_TITLE);              // start with the title-screen segment in the window
	R[1] = MemSeg_GetWindow();           // which segment is showing? -> 16 (SEG_TITLE)

	MemSeg_SetWindow(SEG_LEVEL1);        // flip to the level-1 segment
	R[2] = MemSeg_GetWindow();           // now showing -> 20 (SEG_LEVEL1)

	R[0] = (R[1] == SEG_TITLE && R[2] == SEG_LEVEL1) ? 0xA5 : 0x00;
	for (;;) {}
}
```

Runs to `R[] = a5 10 14` (`0x10 = 16`, `0x14 = 20`). *(tested: `memseg_01_setget.c`)*

---

## Borrow and restore — `MemSeg_Window` / `MemSeg_Restore`

```c
MemSeg MemSeg_Window(MemSeg seg);   // show `seg`, and RETURN the one that was showing
void   MemSeg_Restore(MemSeg prev); // put `prev` back
```

The safe pattern for a *temporary* switch: `MemSeg_Window` maps a new segment and hands you back
the old one, so you can always return to exactly where you were. Anything you read or write at
`0x8000` after the switch hits the newly mapped segment — and each segment is its own physical RAM,
so your home segment keeps its contents while you're away.

```c
// SEGMENT WINDOWING — borrow a segment, then put things back.
//
// Most of the time your game runs from its "home" segment. Now and then you need to
// reach into another one — read a tile from the level-data segment, copy a sprite out
// of an asset segment — and then carry on as before.
//
// MemSeg_Window(seg) swaps a segment into the window AND hands back the one that was
// there, so you can MemSeg_Restore() it afterwards. This is the safe pattern: you can
// never "forget" which segment you were on. Whatever you write through the window at
// 0x8000 goes into REAL mapper RAM — each segment is its own physical memory, so the
// home segment keeps its contents while you are away.
#include "memseg.h"
volatile u8 __at(0xE000) R[4];

#define SEG_HOME    16      // the segment the game normally runs from
#define SEG_ASSETS  17      // a segment we want to peek into for a moment

void main(void)
{
	MemSeg_Init(SEG_HOME);                  // home segment is in the window
	*(volatile u8*)0x8000 = 0xAA;           // store something in home (a health value, say)

	MemSeg prev = MemSeg_Window(SEG_ASSETS); // borrow the assets segment; remember home (prev)
	*(volatile u8*)0x8000 = 0xBB;           // this write lands in SEG_ASSETS, not home
	MemSeg_Restore(prev);                   // done — put the home segment back

	R[1] = prev;                            // prev is 16 (SEG_HOME): we knew where to return
	R[2] = *(volatile u8*)0x8000;           // 0xAA — home never lost its value while we were away
	R[0] = (prev == SEG_HOME && R[2] == 0xAA) ? 0xA5 : 0x00;
	for (;;) {}
}
```

Runs to `R[] = a5 10 aa` — writing `0xBB` while the assets segment was mapped left the home
segment's `0xAA` untouched, because they are physically different RAM. *(tested: `memseg_02_window.c`)*

> Everything you touch at `0x8000..0xBFFF` after a `Window` call hits the mapped segment. The
> [Far Pointers](Far-Pointers.md) API wraps all of this so you deal in handles, not raw window
> addresses — start there unless you specifically need the window yourself.

## See also

- [Far Pointers](Far-Pointers.md) — the everyday API: handles, block copy, and the safe
  scoped-borrow `Far_With`, all built on these primitives.
- [Code Banking](Code-Banking.md) — the same "bigger than 64 KB" idea, but for program *code*.
