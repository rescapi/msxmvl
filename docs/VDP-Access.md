# VDP Access (`vdp`)

The core of the compat layer: reading and writing VRAM and VDP control registers on the V9938
(MSX2). Link `vdp.c`, include `vdp.h`. This page covers the **everyday VRAM-access and register
functions** — the ones nearly every program uses. The module also has sprite, palette, and
table-setup helpers (~59 functions total); those follow the same conventions and are listed in
`vdp.h`.

## Screen modes: what you can draw on

Before anything else you pick a **screen mode**. It decides the resolution, how many colours, how
many bits each pixel costs — and therefore which of the drawing modules will even work.

MSX users know these as SCREEN *n* from BASIC; the VDP documentation calls them TEXT/GRAPHIC. Both
names are defined, and they are the same constants:

| BASIC | `vdp.h` | Resolution | Colours | Notes |
|---|---|---|---|---|
| SCREEN 0 | `VDP_MODE_TEXT1` | 40×24 text | 2 | MSX1 |
| SCREEN 1 | `VDP_MODE_GRAPHIC1` | 32×24 tiles | 16 | MSX1 |
| SCREEN 2 | `VDP_MODE_GRAPHIC2` | 256×192 tiles | 16 | MSX1 — the classic MSX1 game mode |
| SCREEN 3 | `VDP_MODE_MULTICOLOR` | 64×48 blocks | 16 | MSX1 |
| SCREEN 4 | `VDP_MODE_GRAPHIC3` | 256×192 tiles | 16 | MSX2 |
| **SCREEN 5** | **`VDP_MODE_GRAPHIC4`** | **256×212** | **16** | **MSX2 bitmap — the usual choice** |
| SCREEN 6 | `VDP_MODE_GRAPHIC5` | 512×212 | 4 | MSX2 bitmap |
| SCREEN 7 | `VDP_MODE_GRAPHIC6` | 512×212 | 16 | MSX2 bitmap |
| SCREEN 8 | `VDP_MODE_GRAPHIC7` | 256×212 | 256 | MSX2 bitmap |

Setting one is a single call:

```c
#include "vdp.h"

VDP_SetMode(VDP_MODE_GRAPHIC4);     // SCREEN 5: 256x212, 16 colours
```

**Which mode should you pick?** For an MSX2 game, **SCREEN 5** (`GRAPHIC4`) is the default answer:
it is a true bitmap, 16 colours from a 512-colour palette, and it leaves enough VRAM for **two full
pages** — which is what [Double Buffering](Double-Buffering.md) needs for flicker-free animation.
SCREEN 8 gives you 256 colours but eats twice the VRAM and halves your fill rate.

**The mode decides what works.** The V9938 **command engine** — the hardware blitter behind
[Bitmap Drawing](Bitmap-Drawing.md), [Tilemaps](Tilemaps.md) and [Hardware Scroll](Hardware-Scroll.md)
— only runs in the **bitmap modes (SCREEN 5–8)**. Try to `Draw_FillBox` in SCREEN 2 and nothing
happens. If you are targeting an MSX1, you have SCREEN 0–3 only, and none of those modules apply;
see the machine-support table on the [home page](Home.md).

## VRAM addressing: 16K vs 128K

The V9938 has up to 128 KB of VRAM. Two families of accessors:

- **`*_16K`** — 14-bit address (`0x0000..0x3FFF` within the current 16 KB page). This is the
  everyday range and what all examples here use.
- **`*_128K`** — full 17-bit address (`destLow` + `destHigh`) for reaching all 128 KB, e.g. the
  extra pages used by SCREEN 7/8 double buffering.

VRAM is not CPU-addressable — you go through the VDP data port, so a "peek" is a real VDP
transaction, not a memory read. The functions handle the port protocol (address latch + `di`/`ei`
where needed) for you.

---

## `VDP_FillVRAM_16K` / `VDP_Peek_16K`

```c
void VDP_FillVRAM_16K(u8 value, u16 dest, u16 count);
u8   VDP_Peek_16K(u16 src);
```

Fill a run of VRAM with one byte; read a single byte back. Filling is the fast way to clear a
tile map, a bitmap region, or a table.

```c
#include "vdp.h"
volatile u8 __at(0xE000) R[4];

void main(void)
{
	VDP_FillVRAM_16K(0x5A, 0x1000, 16);     // VRAM[0x1000..0x100F] = 0x5A

	R[1] = VDP_Peek_16K(0x1000);            // 0x5A (first)
	R[2] = VDP_Peek_16K(0x100F);            // 0x5A (last)
	R[0] = (R[1] == 0x5A && R[2] == 0x5A) ? 0xA5 : 0x00;
	for (;;) {}
}
```

Runs to `R[] = a5 5a 5a`. *(tested: `vdp_01_fillpeek.c`)*

---

## `VDP_Poke_16K` / `VDP_Peek_16K`

```c
void VDP_Poke_16K(u8 val, u16 dest);
u8   VDP_Peek_16K(u16 src);
```

Single-byte write/read. Convenient for occasional touches; for runs, prefer `Fill`/`Write` which
set the VRAM address once and stream (each `Poke`/`Peek` re-latches the address).

```c
#include "vdp.h"
volatile u8 __at(0xE000) R[4];

void main(void)
{
	VDP_Poke_16K(0x3C, 0x0200);            // VRAM[0x0200] = 0x3C
	VDP_Poke_16K(0xC3, 0x0201);            // VRAM[0x0201] = 0xC3

	R[1] = VDP_Peek_16K(0x0200);           // 0x3C
	R[2] = VDP_Peek_16K(0x0201);           // 0xC3
	R[0] = (R[1] == 0x3C && R[2] == 0xC3) ? 0xA5 : 0x00;
	for (;;) {}
}
```

Runs to `R[] = a5 3c c3`. *(tested: `vdp_02_pokepeek.c`)*

---

## `VDP_WriteVRAM_16K` / `VDP_ReadVRAM_16K`

```c
void VDP_WriteVRAM_16K(const u8* src, u16 dest, u16 count);
void VDP_ReadVRAM_16K(u16 src, u8* dest, u16 count);
```

Block transfer between a RAM buffer and VRAM — how you upload tile/sprite/bitmap data and read it
back. One address setup, then a streamed run.

```c
#include "vdp.h"
volatile u8 __at(0xE000) R[8];

void main(void)
{
	const u8 tile[4] = { 0x12, 0x34, 0x56, 0x78 };
	u8 back[4];

	VDP_WriteVRAM_16K(tile, 0x0800, 4);    // upload 4 bytes to VRAM 0x0800
	VDP_ReadVRAM_16K(0x0800, back, 4);     // read them back into RAM

	R[1] = back[0]; R[2] = back[1]; R[3] = back[2]; R[4] = back[3];
	R[0] = (back[0]==0x12 && back[1]==0x34 && back[2]==0x56 && back[3]==0x78)
	       ? 0xA5 : 0x00;
	for (;;) {}
}
```

Runs to `R[] = a5 12 34 56 78`. *(tested: `vdp_03_writeread.c`)*

> **128 KB variants.** `VDP_WriteVRAM_128K` / `VDP_ReadVRAM_128K` / `VDP_Poke_128K` /
> `VDP_Peek_128K` take a split `low`/`high` address to reach all of VRAM — needed for SCREEN 7/8
> and the higher pages. Same idea, wider address.

---

## `VDP_RegWrite`

```c
void VDP_RegWrite(u8 reg, u8 value);
```

Write any VDP control register directly — the escape hatch for anything the typed helpers don't
cover. (Many convenience wrappers in `vdp.h` are thin `VDP_RegWrite` calls, e.g. `VDP_SetColor(c)`
is `VDP_RegWrite(7, c)`.)

```c
#include "vdp.h"
volatile u8 __at(0xE000) R[2];

void main(void)
{
	VDP_RegWrite(7, 0x0A);         // R#7 = backdrop color 10
	R[0] = 0xA5;                   // reached here; R#7 value checked by the harness
	for (;;) {}
}
```

The harness reads VDP R#7 back from hardware: `0x0A`. *(tested: `vdp_04_regwrite.c`, with
`EX_VREG_ASSERT="7=0a"`)*

## Beyond the core

`vdp.h` also provides: mode setup (`VDP_SetMode`, `VDP_Initialize`), palette
(`VDP_SetPaletteEntry`, `VDP_SetDefaultPalette`), sprites (`VDP_SetSprite`,
`VDP_LoadSpritePattern`, `VDP_SetSpritePosition`), and table pointers (`VDP_SetLayoutTable`,
`VDP_SetPatternTable`, …). They follow the same calling conventions shown above.

## MSX1: the VDP is slower, and it will not tell you

Every VDP access has a **minimum gap** the chip needs before the next one. Miss it and the VDP
silently **drops the byte** — no error, no status bit; your data just comes out short or shifted.

| Machine | VDP | Minimum gap between VRAM accesses |
|---|---|---|
| MSX2 | V9938 | ~15 T-states |
| MSX1 | TMS9918 | **~29 T-states** |

This bites the *block* transfers, because the fast Z80 block instructions are tuned to the V9938:
`OTIR`/`INIR` move a byte every **21 T** — comfortably above 15, but well under 29. On an MSX1 that
is too fast, and bytes vanish.

`vdp.c` therefore compiles **padded loops** (30–33 T/byte) when you build with
`-DMSX_VERSION=MSX_1`, for `VDP_WriteVRAM_16K`, `VDP_ReadVRAM_16K` and `VDP_FillVRAM_16K`. It is a
`#if`, not a runtime test: **an MSX2 build is byte-identical to one with no MSX1 support**, and
pays nothing.

Two things worth internalising, because they cost real debugging time:

- **A dropped byte in a `Fill` is invisible.** Every byte written is the same, so corrupted output
  looks fine — the run just ends early. A fill-then-peek test passes even when the fill is broken.
  This is precisely why `VDP_FillVRAM_16K` needed fixing despite its test being green.
- **`VDP_FastFillVRAM_16K` stays blanked-only** on both machines. At ~12.8 T/byte it is far below
  even the V9938's minimum; it is only legal with the display off, where the VDP gives the CPU full
  VRAM bandwidth.

*(All four VDP examples above are also run on an emulated MSX1 by `examples/run_all_msx1.sh`.)*

## See also

- [Double Buffering](Double-Buffering.md) — the `display` extension builds page flipping on top of
  the VDP.
- [Text Output](Text-Output.md) — the `print` module renders text into VRAM.
