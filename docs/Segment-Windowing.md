# Segment Windowing (`memseg`)

This is the machinery underneath [Far Pointers](Far-Pointers.md) — the raw way to swap 16 KB
**banks** of mapper RAM in and out of the Z80's view. Most code should use the friendlier
[Far Pointers](Far-Pointers.md) API and never come here. Reach for `memseg` directly only when you
want to map a bank yourself and run your own tight loop against it.

> Link `memseg.c`, include `memseg.h`.

## What "windowing" means

The Z80 addresses 64 KB, split into four 16 KB **pages**. A RAM mapper has many more 16 KB **banks**
than that (512 KB = 32 banks), and it lets you choose which bank appears in a page — like sliding
one interchangeable cartridge at a time into a slot you can see through. That visible slot is the
**window**. `memseg` uses **page 2** (`0x8000`–`0xBFFF`) for it: your program runs in page 1 and
your stack lives in page 3, so page 2 is the safe one to repurpose.

Two hardware facts shape the whole API:

- **The mapper register is write-only.** You can tell the hardware "show bank 20 now", but you can't
  ask it "which bank is showing?". So `memseg` keeps a **software note** (a "shadow") of the current
  bank, updated every time you change it. That shadow is what makes save-and-restore possible.
- **Init imposes a mapping, it doesn't discover one.** Because the boot bank can't be read back,
  `MemSeg_Init` *sets* a known "home" bank rather than detecting whatever was there. From then on the
  shadow always matches the hardware.

> **MSX2 only — and it fails quietly.** An MSX1 has no RAM mapper. On MSX1, `MemSeg_SetWindow` writes
> to a port nothing answers, and `MemSeg_GetWindow` then returns the **shadow** — which faithfully
> reports the bank you *asked* for, even though nothing was actually mapped. The shadow is a record
> of your intent, not a presence check. Only [`farmem`](Far-Pointers.md), which reads back through
> the window, can tell that nothing is there. If you need more *code* (not data) on an MSX1, use
> [Code Banking](Code-Banking.md) instead — MegaROM banking is cartridge hardware and works on every
> MSX.

---

## Pick a bank — `MemSeg_Init` / `MemSeg_SetWindow` / `MemSeg_GetWindow`

Call `MemSeg_Init(home)` once at startup with a "home" bank; then `MemSeg_SetWindow` flips the
visible bank and `MemSeg_GetWindow` tells you which one it is.

```c
// banks.c — flip which 16 KB bank of mapper RAM is visible in the window.
#include "memseg.h"

#define BANK_TITLE   16     // the bank holding the title screen
#define BANK_LEVEL1  20     // ...and the bank holding level 1

void show_title(void)   { MemSeg_SetWindow(BANK_TITLE); }
void show_level1(void)  { MemSeg_SetWindow(BANK_LEVEL1); }
u8   visible_bank(void) { return MemSeg_GetWindow(); }
```

Start with `MemSeg_Init(BANK_TITLE)`, and `visible_bank()` reads back `16`; call `show_level1()` and
it reads back `20`. *(tested: `memseg_01_setget.c`)*

---

## Borrow and restore — `MemSeg_Window` / `MemSeg_Restore`

The safe pattern for a *temporary* switch: `MemSeg_Window` maps a new bank and hands you back the
old one, so you can always return to exactly where you were. Anything you read or write at `0x8000`
after the switch hits the newly mapped bank — and each bank is its own physical RAM, so your home
bank keeps its contents while you're away.

```c
// banks.c — borrow another bank for a moment, then restore yours.
#include "memseg.h"

#define BANK_HOME    16     // the bank the game normally runs from
#define BANK_ASSETS  17     // a bank we want to write into for a moment

// Write one byte into the assets bank, then come straight back to the home bank.
void poke_asset(u8 value)
{
	MemSeg prev = MemSeg_Window(BANK_ASSETS);   // borrow the assets bank; remember the old one
	*(volatile u8*)0x8000 = value;              // this write lands in the assets bank
	MemSeg_Restore(prev);                       // put the old bank back
}
```

After `poke_asset(0xBB)`, the window is back on the home bank and the home bank's own contents are
untouched — the two banks are physically different RAM. *(tested: `memseg_02_window.c`)*

> Everything you touch at `0x8000..0xBFFF` after a `Window` call hits the mapped bank. The
> [Far Pointers](Far-Pointers.md) API wraps all of this so you deal in handles, not raw window
> addresses — start there unless you specifically need the window yourself.

## See also

- [Far Pointers](Far-Pointers.md) — the everyday API: handles, block copy, and the safe
  scoped-borrow `Far_With`, all built on these primitives.
- [Code Banking](Code-Banking.md) — the same "bigger than 64 KB" idea, but for program *code*.
