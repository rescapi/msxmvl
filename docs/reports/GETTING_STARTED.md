# Getting Started with msxmvl

msxmvl ("mostly vibes c z80") is a clean-room MSX2 C library with a 100% MSXgl-compatible API,
tuned to sit as close to hand-written assembly as possible.

## Requirements
- **SDCC 4.2.0+** (the Z80 backend). msxmvl uses the modern `--sdcccall 1` calling convention.
- **hex2bin** (bundled with z88dk) to produce final ROM/COM images.
- An MSX2 emulator for testing — **openMSX** is the reference arbiter used by this project.

## Building against msxmvl
1. Add `lib/gen/` to your include path.
2. `#include` the module headers you need (e.g. `#include "vdp.h"`).
3. Compile with `--sdcccall 1`:
   ```
   sdcc -c -mz80 --sdcccall 1 your_app.c
   sdcc -c -mz80 --sdcccall 1 lib/gen/vdp.c        # link the modules you use
   sdcc -o app.ihx --code-loc 0x4010 --data-loc 0xC000 -mz80 --no-std-crt0 \
        --sdcccall 1 crt0.rel your_app.rel vdp.rel
   hex2bin -e ROM -l 8000 app.ihx
   ```

## Calling convention (important)
msxmvl is built for **`--sdcccall 1`**. Under this convention:
- arg1 → `HL` (16-bit) / `A` (8-bit); arg2 → `DE` (16-bit) / `L` (8-bit); arg3+ → stack.
- returns: `u8` → `A`; `u16`/pointer → `DE` (a few `__FASTCALL` functions return in `HL`); `u32` → `DE:HL`.

If you link msxmvl with `--sdcccall 0`, calls will pass garbage — the convention must match.

## Module map
See [API_REFERENCE.md](API_REFERENCE.md) for every function. Key modules:
- **vdp** — V9938 screens 5–8, command engine (HMMV/HMMM/LMMM), VRAM access.
- **v9990** — V9990/GFX9000 (Graphics9000) support.
- **dos** / **dos_mapper** — MSX-DOS 1 & 2 (BDOS wrappers, FCB + handle file APIs).
- **memory** / **memory_mapper** — fills/copies, dynamic allocator, memory-mapper paging.
- **bios**, **psg**, **msx-music**, **msx-audio**, **scc** — hardware/sound.
- **print**, **string**, **math**, **draw**, **tile**, **sprite_fx**, **game_pawn** — app-level helpers.

## Verifying correctness
Every msxmvl function is differentially tested against the MSXgl original in openMSX. To re-verify a
module yourself, see `compat/verify_rig.sh` (hardware-surface diff), `compat/verify_dos_static.sh`
(static asm diff vs MSXgl's real source), and `compat/README.md`.
