# Proposal: SRAM game saves (FM-PAC/PAC + ASCII mapper-SRAM cartridges)

**Status:** IMPLEMENTED — `lib/ext/sram.{h,c}` (block API, both backends), `lib/gen/pac.{h,c}`
(MSXgl-compatible surface over the same core), bankpack `SRAM <kb>` manifest line. User docs:
[Game-Saves](Game-Saves.md). Verified: `docs/examples/sram_01_pac.c` + `pac_01_compat.c`
(openMSX `pac` extension, C-BIOS MSX1+MSX2) and `test/banksram/run.sh` (ASCII8SRAM8 /
ASCII16SRAM2 / ASCII8SRAM32, persistence across a second boot, manifest negatives).
This page remains the hardware ground truth the implementation was built against.

## Why

Game saves are the demand. Today the library's save story is: 6 bytes of RTC CMOS
(`RTC_SaveData`), password codes (`crypt`), or a disk drive (`disk`) — nothing for a
cartridge game on a diskless MSX. Real MSX hardware has two battery-backed answers:

1. **PAC / FM-PAC** (Panasoft SW-M001 / SW-M004): a *separate* cartridge with an 8 KB
   SRAM chip, usable by any software that speaks its unlock protocol. Many commercial
   games saved to it.
2. **Mapper-SRAM cartridges**: the game's *own* ASCII8/ASCII16 MegaROM with an SRAM chip
   wired into the mapper (Hydlide 2, Daisenryaku, A-Train, the Koei titles, Wizardry;
   Konami's Game Master 2 is the Konami-mapper cousin). Since bankpack builds ASCII8/16
   MegaROMs, this is *our* cartridge format growing a save file.

MSXgl covers only #1 (`device/pac.c`: slot scan, activate, 8×1 KB page carve with
read/write/format/check and an optional 4-byte app signature — all via per-byte interslot
BIOS calls). It has **no mapper-SRAM support at all**, so #2 is differentiation, and it
composes directly with our banking stack.

## Hardware ground truth (openMSX device sources = emulation contract)

**PAC/FM-PAC protocol** — the SRAM sits at `0x4000-0x5FFD` (8190 usable bytes) in the
PAC's *slot*, page 1. Write `'M'` (0x4D) to `0x5FFE` and `'i'` (0x69) to `0x5FFF` to
unlock it (both readable back as written); write anything else to either to relock (ROM,
if any, reappears). Detection = for each (sub)slot: relock, verify `0x5FFD` is not
writable; unlock, verify it is; restore all three bytes. The FM-PAC additionally has
`0x7FF7` (16 KB ROM bank select) and `0x7FF6` (OPLL enable) — irrelevant to saving.

**ASCII mapper-SRAM** — SRAM is an *extra segment* behind the normal bank-select
registers. Writing a select value with the **SRAM bit** set maps SRAM instead of ROM
into that window; low bits pick the 8 KB SRAM block (ASCII8). Writes reach SRAM only
while it's mapped (in practice: use the `0x8000` page — the openMSX ASCII16 variant
write-enables *only* there, and plain-ASCII8 variants only `0x8000-0xBFFF`).

| Variant | SRAM-select value | Size | openMSX `-romtype` |
|---|---|---|---|
| ASCII8 + SRAM | `enable_bit \| block`, enable_bit = rom_kb/8 (first bit above ROM banks) | 2/8/32 KB | `ASCII8SRAM2/8/32` |
| Koei | same, SRAM also mappable at `0x4000` | 8/32 KB | `KoeiSRAM8/32` |
| Wizardry | fixed `0x80 \| block` | 8 KB | `Wizardry` |
| ASCII16 + SRAM | exactly `0x10` (no block bits; SRAM mirrors in the 16 K window) | 2/8 KB | `ASCII16SRAM2/8` |
| Game Master 2 | Konami mapper, bit4=SRAM, bit5=4K half, writes at `0xB000` | 8 KB | `GameMaster2` (not a bankpack target) |

**Compatibility verdict: the two protocols are fundamentally different.** PAC is a
magic-byte unlock of a fixed page-1 range in a *foreign slot* (interslot access, works
from any program, zero link-time knowledge); mapper-SRAM is a segment select inside *your
own cartridge* (needs the mapper type, ROM size, and window discipline baked in at build
time, but is then just a fast local `ldir`). No register, address range, or discovery
mechanism is shared. A unified API is therefore only honest at the **block level** —
detect / size / read / write a save record — with the mapping mechanics per backend,
exactly the `memseg` backend pattern.

## What to build: `lib/ext/sram` (one API, compile-time backend)

```c
bool Sram_Detect(void);              // PAC: slot scan (magic protocol above);
                                     // MAPPER: map SRAM, RAM-test a byte, restore
u16  Sram_Size(void);                // usable bytes: PAC 8190; MAPPER 2048/8192/...
void Sram_Read (u16 ofs, void* dst, u16 n);   // open + copy + close inside
void Sram_Write(u16 ofs, const void* src, u16 n);
bool Sram_Verify(u16 ofs, const void* src, u16 n); // read-back compare (battery health)
```

- **Backend `SRAM_BACKEND_PAC`** — v1 does per-byte `RDSLT`/`WRSLT` interslot access
  (C-BIOS has both; ROM code can't just switch page 1 — it's executing there). Always
  relock (`0x5FFF ← 0`) on the way out so a crash can't scribble on saves. A later
  RAM-stub fast path (copy loop above `0x8000`, `di`, page-1 slot switch) is optional.
- **Backend `SRAM_BACKEND_MAPPER`** — map SRAM into the `0x8000` window via the bank-2
  register (`0x7000`, same for ASCII8/16): `di`, save current segment **from the SHADOW
  byte** (registers are write-only — the memseg discipline), write the SRAM-select
  value, `ldir`, restore SHADOW's segment, `ei`. Interrupts stay disabled while mapped:
  an ISR far-calling through the same window would land in SRAM. No open/close is
  exposed; the block APIs are the whole surface, so a stale-mapping bug class can't exist.
- **Record header** (both backends, optional like `PAC_USE_SIGNATURE`): app id + length +
  checksum at `ofs`, so `Sram_Detect`+`Sram_Verify` can distinguish "our save" / "someone
  else's data" / "empty (0xFF)" — a PAC is shared between games by design.
- **bankpack manifest**: a `SRAM <kb>` line (requires `MAPPER ASCII8|ASCII16`) emits
  `SRAM_SELECT`/`SRAM_SIZE` constants for the module — ASCII8: `rom_kb/8` (overridable
  for Wizardry-style fixed wiring), ASCII16: `0x10`, sizes validated (ASCII16: 2/8 only).
  The `.rom` image itself is unchanged — SRAM is a chip on the cart, not file content.
  RESERVE-style guard: reject a `SRAM` line whose select value collides with a code
  segment number (can't happen when rom_kb is honest; check it anyway).

## Acceptance

- **CI-safe (the big win)**: the openMSX `pac` extension is the SRAM-only SW-M001 —
  **no copyrighted ROM needed**, unlike `fmpac`. So the PAC backend tests run headless on
  C-BIOS in CI: `EX_EXT=pac` + detect/write/read-back/relock asserts, sentinel at 0xE000.
- **Mapper backend in CI too**: boot the bankpack test ROM with
  `-romtype ASCII8SRAM8` (and an ASCII16 twin with `ASCII16SRAM2`); assert write/read
  through the window, ROM segment correctly restored after, and (locally) persistence
  across a second boot via openMSX's `.pac`-style SRAM file.
- **fmpac extension**: local-only, exactly the `msxmusic_01_note.c` precedent in
  `docs/examples/run_all.sh` (CI skip: copyrighted `fmpac.rom`); verifies the same PAC
  code path against the combined FM+SRAM device.

## Docs

- New wiki page `Game-Saves.md` (or fold into this page): both backends, the shared-PAC
  etiquette (signature, never format pages you didn't sign), `run_all.sh` entries.
- `Code-Banking.md` + `tools/README.bankpack.md`: the `SRAM` manifest line, window rules.

## Priority + effort

Medium priority, small effort: the PAC backend is ~150 lines of C (MSXgl's is the
existence proof), the mapper backend is smaller still (one `ldir` bracketed by the
memseg discipline we already ship), and the bankpack change is a manifest line plus two
constants. Do it when the first game needs saves — but the spec is now paid for, and the
CI story (ROM-free `pac` extension + `-romtype` SRAM variants) means it can be fully
verified the day it's written, which fits the repo goal of *easier to use, proven on
emulated hardware*.
