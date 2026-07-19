# Asset Pipeline (`img2tiles`)

Turn your own art into MSX data. `tools/img2tiles.py` converts a PNG into an **MSX
SCREEN 2 (GRAPHIC 2) tileset** — pattern table + colour table — plus a
**de-duplicated name table** (tilemap), emitted as C arrays you `#include` directly.
Minimal and dependency-light: Python + Pillow, deterministic, golden-tested. No
vendored SDK, no compiled C++ converter.

```sh
python3 tools/img2tiles.py  art.png  myart  myart_tiles.h
```

Produces `myart_tiles.h`:

```c
#include "types.h"
const u8 myart_pattern[...];     // 8 bytes / unique tile (1 bit/pixel)
const u8 myart_colour[...];      // 8 bytes / unique tile (hi nibble fg, lo nibble bg)
const u8 myart_nametable[...];   // tw*th tile indices (the de-duplicated tilemap)
#define MYART_NTILES n
#define MYART_W tw
#define MYART_H th
```

## Format

SCREEN 2 stores each 8×8 tile as **8 pattern bytes** (one per pixel row, bit set =
foreground) and **8 colour bytes** (one per row: high nibble = foreground colour, low
nibble = background colour, from the 16-colour TMS9918 palette). The hardware allows
**at most two colours per 8-pixel row** — `img2tiles` enforces this by reducing each
row to its two most-used palette colours (others snapped to the nearer of the two).
Input pixels are matched to the nearest of the standard 16-colour palette.

The image width and height must both be multiples of 8. Identical tiles (same
pattern **and** colour) collapse to one entry; the name table references them, so
repeated art costs nothing (a full-screen import of a tiled background is typically a
large saving — reported on conversion).

## Bitmap modes — SCREEN 5/7/8 (`img2bitmap`)

For MSX2 bitmap screens, `tools/img2bitmap.py` converts a PNG into **linear VRAM
data** you copy straight to the screen — no tiles, no name table.

```sh
python3 tools/img2bitmap.py  photo.png  photo  8   photo_bitmap.h   # SCREEN 8
python3 tools/img2bitmap.py  title.png  title  5   title_bitmap.h   # SCREEN 5
```

| Screen | Mode | Depth | Byte layout |
|---|---|---|---|
| **5** (GRAPHIC 4) | 256×212, 16 colours | 4 bpp | 2 pixels/byte (hi nibble = left) |
| **7** (GRAPHIC 5) | 512×212, 16 colours | 4 bpp | 2 pixels/byte |
| **8** (GRAPHIC 6) | 256×212, 256 colours | 8 bpp | 1 pixel/byte, fixed **GRB332** |

SCREEN 5/7 pixels are 4-bit indices into a programmable 16-colour palette; the tool
quantises to the **MSX2 default palette** and also emits `<name>_palette[32]` in VDP
register format (2 bytes/colour: `0RRR0BBB`, `00000GGG`) so you can load it. SCREEN 8
has a fixed hardware palette (3 bits green, 3 red, 2 blue), so no palette is emitted —
each byte is the colour directly. Output alongside the bitmap: `#define <NAME>_W/_H/
_BPP/_SCREEN`.

### Optimal palette (`--optimal`)

The fixed default palette is fine for logos but poor for photographs and gradients.
Add `--optimal` (SCREEN 5/7 only) to derive a **per-image 16-colour palette** by
median-cut in the MSX2 9-bit (512-colour) space — a large quality jump for real art:

```sh
python3 tools/img2bitmap.py  photo.png  photo  5  photo_bitmap.h  --optimal
```

The emitted `<name>_palette[32]` is that adapted palette (same VDP format); load it
before drawing the bitmap. Median-cut is deterministic (weighted, tie-broken by
colour), so the same PNG always yields the same palette and bytes.

## Maths tables — sin / cos / atan (`gentables`)

Games need trig, and the Z80 has no FPU — so you bake fixed-point tables at build
time. `tools/gentables.py` emits them as C arrays using the demo-scene **binary
angle**: one full turn is 256 steps by default, so an angle fits in a `u8` and wraps
for free.

```sh
python3 tools/gentables.py  sin   sintab            # i8[256], amp 127
python3 tools/gentables.py  cos   costab            # i8[256] (sine shifted a quarter)
python3 tools/gentables.py  atan  atantab           # u8[256], slope->angle
python3 tools/gentables.py  sin   sin88  256  256   # i16[256], 8.8 fixed (amp 256)
```

`sin`/`cos` produce `amp*sin(2π·i/entries)` — signed, `i8` when `amp ≤ 127` (the
compact classic table) or `i16` above. `atan` maps a slope `i/entries` in `[0,1)` to
its angle in the same brad unit (combine with octant reflection for a full `atan2`).
Each file also `#define`s `<NAME>_ENTRIES`. Pure Python, no numpy.

## Correctness

Every tool is deterministic: the converters are self-inverse (after emitting they
re-derive the data from the source and assert it matches — a bug fails the conversion,
it never ships silently), and the table generator is a pure function of its arguments.
The drift guards run in CI via `docs/examples/run_tools.sh`:
`img2tiles_test.py` (round-trip + dedup 8→2 + golden), `img2bitmap_test.py` (SCREEN 5
4bpp+palette & SCREEN 8 GRB332 round-trip + golden), and `gentables_test.py` (sin/cos
quarter points, atan shape, i8/i16 selection + golden).

## Roadmap

D1 (SCREEN 2 tiles), D2 (SCREEN 5/7/8 bitmaps), D3 (sin/cos/atan tables) and D4
(`--optimal` median-cut palette) are done. Planned extensions (same tools): ZX0-packing
of the output (consuming the `unzx0` decoder) and 8×8/16×16 sprite banks.

## See also
- [VDP Access](VDP-Access.md) — pushing the pattern/colour/name tables into VRAM.
- [Tilemaps](Tilemaps.md) — drawing with the name table on-screen.
- [Compression](Compression.md) — `UnZX0` to unpack a ZX0-packed tileset at load time.
