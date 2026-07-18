# Documentation examples — tested source

Every code example shown in the [msxmvl documentation](../Home.md) lives here as a real,
self-contained program, compiled to a 16 KB MSX2 ROM and run headless on C-BIOS under
openMSX. **If an example appears in the documentation, it passed here.**

## Contract

Each `*.c` is a complete program that writes a result byte to `R[0]` at RAM `0xE000`:
`0xA5` = all checks passed. Extra `R[1..]` bytes carry observed values (shown after the
snippet in the documentation). The harness boots the ROM, waits for `main` to run, and reads `R[]`.

## Run

```sh
./run_all.sh                       # every documented example (MSX2) + the drift guard
./run_all_msx1.sh                  # the MSX1-capable subset, on an emulated MSX1
./run_example.sh <file.c> "<modules>" [expect_r0=a5] [ramlen=8]
./verify_docs.sh                   # assert the documentation code == these files (no drift)
```

`EX_MACHINE=C-BIOS_MSX1` builds an example with `-DMSX_VERSION=MSX_1` and boots it on an MSX1.
That is how the documentation's MSX1 claim is backed: 23 examples are actually run there, and the modules
that genuinely need a V9938 or the MSX2 RAM mapper are excluded rather than fudged. See the header
of `run_all_msx1.sh` for the two examples that pass *vacuously* on MSX1 and must not be added.

`run_example.sh` links only the modules you name (from `lib/ext/` or `lib/gen/`), so each
example is a minimal, honest reproduction of what the documentation claims.

`assets/` holds binary files some examples use (the MoonBlaster example song + sample kit,
by C.v/d Geest, included with permission); the checked-in `moonblaster_song.h` /
`moonblaster_kit.h` arrays are generated from them by `tools/mb14blob.sh`. `run_all.sh` finishes by
running `verify_docs.sh`, which fails if any executable line drifts from its documentation page.

## `crt_init.c` — why it exists

The vendor Fusion-C cart crt0 (`crt0_MSX816kROM4000.rel`) jumps straight to `main` without
performing either half of the C startup contract a real MSX ROM crt0 does (compare MSXgl's
`crt0_rom16.asm`, which calls `INIT_GLOBALS`):

1. **zero `_DATA`** — otherwise `static u8 n;` starts at whatever was left in RAM;
2. **copy `_INITIALIZER` (ROM) → `_INITIALIZED` (RAM)** — otherwise `static u8 x = 5;` reads
   back as garbage.

Worse, because that crt0 never *declares* `_INITIALIZER`, the linker parks it right after
`_DATA` — in RAM, so the initializer bytes are not even in the ROM image.

`crt_init.c` supplies `main()`, does both steps, then calls the example (compiled with
`-Dmain=app_main`). `run_example.sh` links it first and pins `_INITIALIZER` just past the code
inside ROM, asserting afterwards that it really landed there. Examples can then use ordinary
initialized globals — as they would in any correctly-started ROM program.

Read-only tables should still be `const`: those live in ROM and cost no RAM at all.
