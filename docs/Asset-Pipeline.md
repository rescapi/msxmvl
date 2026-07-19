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

For MSX2 / MSX2+ bitmap screens, `tools/img2bitmap.py` converts a PNG into **linear
VRAM data** you copy straight to the screen — no tiles, no name table.

```sh
python3 tools/img2bitmap.py  photo.png  photo  8   photo_bitmap.h   # SCREEN 8
python3 tools/img2bitmap.py  title.png  title  5   title_bitmap.h   # SCREEN 5
```

| Screen | Mode | Depth | Byte layout |
|---|---|---|---|
| **5** (GRAPHIC 4) | 256×212, 16 colours | 4 bpp | 2 pixels/byte (hi nibble = left) |
| **7** (GRAPHIC 5) | 512×212, 16 colours | 4 bpp | 2 pixels/byte |
| **8** (GRAPHIC 6) | 256×212, 256 colours | 8 bpp | 1 pixel/byte, fixed **GRB332** |
| **12** | 256×212, ~19268 colours (MSX2+) | 8 bpp | 1 byte/pixel, **YJK** (4-pixel chroma groups) |

SCREEN 5/7 pixels are 4-bit indices into a programmable 16-colour palette; the tool
quantises to the **MSX2 default palette** and also emits `<name>_palette[32]` in VDP
register format (2 bytes/colour: `0RRR0BBB`, `00000GGG`) so you can load it. SCREEN 8
has a fixed hardware palette (3 bits green, 3 red, 2 blue), so no palette is emitted —
each byte is the colour directly. Output alongside the bitmap: `#define <NAME>_W/_H/
_BPP/_SCREEN`.

### YJK — thousands of colours (SCREEN 12)

```sh
python3 tools/img2bitmap.py  photo.png  photo  12  photo_bitmap.h   # ~19268 colours
```

SCREEN 12 (V9958 / MSX2+) is **YJK**: every group of 4 horizontal pixels shares a
6-bit chroma pair (J, K) while each pixel keeps its own 5-bit luminance (Y), for about
19268 colours. The tool encodes RGB→YJK (`R=Y+J, G=Y+K, B=(5Y−2J−K)/4`) into one byte
per pixel — no palette. It is lossy (shared chroma), so it shines on **photographs and
smooth gradients**, less so on hard-edged pixel art. Width must be a multiple of 4.
Enter the mode at runtime with `VDP_SetMode(VDP_MODE_GRAPHIC7)` + `VDP_SetYJK(VDP_YJK_ON)`,
then copy `<name>_bitmap` to VRAM.

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

## Hardware sprites (`img2sprites`)

Turn a sprite sheet into an **MSX hardware-sprite pattern table**. `tools/img2sprites.py`
reads a PNG and emits the pattern bytes plus a per-sprite colour.

```sh
python3 tools/img2sprites.py  hero.png   hero    16   hero_sprites.h    # 16x16 sprites
python3 tools/img2sprites.py  bullets.png bullet  8   bullet_sprites.h  # 8x8 sprites
```

MSX sprites are **monochrome per pattern**: a pixel is *on* (drawn in the sprite's
colour) or *off* (transparent). A pixel is off if it is transparent (alpha < 128) or —
for images with no alpha — **pure black**. Each `size`×`size` cell of the sheet becomes
one sprite:

- **8×8** → 8 bytes/sprite (one per row, bit 7 = leftmost pixel).
- **16×16** → 32 bytes/sprite in the MSX quadrant order **TL, BL, TR, BR** (each an 8×8
  block) — exactly what the VDP expects, so you copy `<name>_pattern` straight to the
  sprite pattern table.

`<name>_colour[count]` holds each sprite's colour (the nearest TMS9918 palette index of
its most-common lit pixel) for the SCREEN 2 sprite-mode-1 attribute byte. Plus
`#define <NAME>_COUNT/_SIZE/_PLANES` (`_PLANES` is 1 here).

### Multi-colour sprites — `--mode2` (OR colour)

MSX sprites are one colour per pattern, but **MSX2 sprite mode 2** (SCREEN 4–8) builds
a multi-colour sprite by **stacking monochrome planes** at the same position: each
plane's per-line colour byte has a **CC (colour-combination) bit** — when set, the VDP
gives that plane the same priority as the nearest lower-numbered plane whose CC=0, and
**OR-combines their colour codes where dots overlap**. `--mode2` produces that layout:

```sh
python3 tools/img2sprites.py  hero.png  hero  16  hero_sprites.h  --mode2
```

Each cell is decomposed into one monochrome plane per distinct colour (a **uniform**
`<NAME>_PLANES` count across the sheet, so plane `p` of sprite `s` is at index
`s*PLANES + p`). The **base plane** (lowest palette index) gets colour byte `colour`
with **CC=0**; the **stacked planes** get `colour | 0x40` (**CC=1**). The colour byte is
`EC(7) | CC(6) | IC(5) | 0 | colour(3-0)` — the caller writes it to all 16 lines of that
plane's mode-2 colour table, and places the `PLANES` consecutive sprite planes at the
same X/Y. Mode 2 allows 8 sprites per horizontal line, so keep combined sprites to a few
planes. (Refs: MSX2 Technical Handbook ch.4; msx.org "The OR Color".)

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

## Packing the output (`--zx0`)

ROM space, not CPU, is the MSX constraint — a SCREEN 8 image alone is 54 KB. Add `--zx0`
to any of the image converters to emit the **bulk arrays ZX0-compressed**, unpacked at
load time by [`UnZX0`](Compression.md):

```sh
python3 tools/img2bitmap.py  photo.png  photo  8  photo.h  --zx0
  → const u8 photo_bitmap_zx0[18904] = {...};   // was 54272 B — 65% saved
    #define PHOTO_BITMAP_LEN 54272              // unpacked length
```

**Compression runs on the host, at build time, once** — using Einar Saukas's reference
`zx0` encoder (the format `UnZX0` decodes). **The MSX only decompresses.** So the ROM
carries the small packed blob plus the tiny decoder, and you unpack at load:

```c
#include "unzx0.h"
UnZX0_VRAM(photo_bitmap_zx0, 0x0000, PHOTO_BITMAP_LEN); // stream straight to VRAM,
                                                        // no full-size RAM buffer
UnZX0(myart_pattern_zx0, ram_buffer);                   // …or unpack into RAM
```

Only the bulk arrays are packed; small metadata stays raw so it's usable immediately:

| Tool | packed (`…_zx0` + `…_LEN`) | left raw |
|---|---|---|
| `img2bitmap` | `bitmap` | `palette` |
| `img2sprites` | `pattern` | `colour` |
| `img2tiles` | `pattern`, `colour`, `nametable` | — |

`--zx0` is opt-in per asset: if your data already fits, don't compress it (you'd only
add load-time latency). It needs the reference `zx0` encoder available at author time
(on `PATH`, or built from the reference source via `$ZX0_SRC`) — nothing extra ships or
runs on the MSX.

## Correctness

Every tool is deterministic: the converters are self-inverse (after emitting they
re-derive the data from the source and assert it matches — a bug fails the conversion,
it never ships silently), and the table generator is a pure function of its arguments.
The drift guards run in CI via `docs/examples/run_tools.sh`:
`img2tiles_test.py` (round-trip + dedup 8→2 + golden), `img2bitmap_test.py` (SCREEN 5
4bpp+palette, SCREEN 8 GRB332, SCREEN 12 YJK tolerance + golden), `img2sprites_test.py`
(exact 8×8 + 16×16 patterns incl. the quadrant order, colour, transparency + golden),
and `gentables_test.py` (sin/cos quarter points, atan shape, i8/i16 selection + golden).
`--zx0` packing is an author-time step (it needs the reference encoder), so its
round-trip test `zx0pack_test.py` runs locally and **skips in CI** — its output is
already covered there by the raw-array goldens plus `unzx0`'s on-target codec corpus.

## Roadmap

D1 (SCREEN 2 tiles), D2 (SCREEN 5/7/8 **and SCREEN 12 YJK** bitmaps), D3 (sin/cos/atan
tables), D4 (`--optimal` median-cut palette), **8×8/16×16 sprite banks** (`img2sprites`)
and **`--zx0` output packing** (consuming the `unzx0` decoder) are done — the asset
pipeline's planned scope is complete.

## See also
- [VDP Access](VDP-Access.md) — pushing the pattern/colour/name tables into VRAM.
- [Tilemaps](Tilemaps.md) — drawing with the name table on-screen.
- [Sprite Transforms](Sprite-FX.md) — flipping/masking the sprite patterns `img2sprites` emits.
- [Compression](Compression.md) — `UnZX0` to unpack a ZX0-packed tileset at load time.
