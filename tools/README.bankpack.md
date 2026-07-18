# bankpack

Turns "banked code" into a real **ASCII-8 MegaROM**. Pure bash + the SDCC toolchain
(`sdldz80`, `sdasz80`, `sdobjcopy`) + coreutils. **No Python.** Two modes:

```sh
bankpack.sh --gen <manifest> [outdir]      # generate far.h + far thunks
bankpack.sh <manifest> <out.rom> [rom_kb]  # link banks + assemble the ROM (rom_kb=64)
```

Runs in the **current directory**: reads the `.rel` files named in the manifest and
writes intermediates (`_bp_*.ihx/.map/.noi/.bin`) there.

## Turnkey flow

The developer writes plain C + a manifest; bankpack generates the glue:

1. `bankpack --gen bank.manifest .` → **`far.h`** (call declarations) and
   **`_bp_far_thunks.asm`** (resident thunks + dispatch table).
2. compile/assemble: `sdasz80` the runtime (`lib/ext/farrt.asm`) + `_bp_far_thunks.asm`;
   `sdcc -c` the banks and `main` (they `#include "far.h"`).
3. `bankpack bank.manifest game.rom` → link each bank, patch the dispatch table,
   assemble the ROM.

Caller-side, a cross-bank call is just `far_<name>(args)`; the bank *defines*
`<name>(...)` normally. See `test/farturnkey/` for an all-C worked example.

## Manifest

Whitespace-separated; `#` comments and blank lines ignored.

```
# SEG  RUN      OBJ [OBJ ...]
  0    0x4000   resident.rel          # <- first line = RESIDENT bank
  4    0x8000   physics.rel helpers.rel
  5    0x8000   render.rel
```

- **SEG** — ASCII-8 segment number (8 KB each); file offset = `SEG * 0x2000`.
- **RUN** — address the bank is *linked to run at* (`0x4000` resident; `0x8000`
  for the bank-2 window).
- **OBJ** — one or more `.rel` files linked into that bank.

The **first line is the resident bank**: linked first, and the only bank that may
be referenced by name from the others.

Data-model keywords (all optional):

```
RAMBASE 0xC000          # statics RAM base (default 0xC000)
RESERVE 0xC800 0xC880   # program-owned RAM [lo,hi): build FAILS on collision
SHADOW  0xE020          # memseg shadow byte (env BANKPACK_SHADOW overrides)
```

### C-runtime data model (statics in banks)

Every bank may have writable C statics — initialized data and BSS. bankpack makes
the crt0 contract hold across banks (spec: `docs/Code-Banking-Data-Model.md`):

1. The resident links its `_DATA`/`_INITIALIZED` at `RAMBASE`; each later bank
   links with an explicit `-b _DATA` in the **next disjoint RAM slice** (the build
   prints a `statics:` line per bank). A generated area-order module is linked
   first in every bank so `_INITIALIZER` lands in the bank's ROM, after its code.
2. bankpack **patches `_bp_datatab`** (in `farrt.asm`, capacity 32 entries) with
   the BSS union `[RAMBASE,end)` and, per bank with initialized statics, its
   in-window ROM source, RAM dest, and length.
3. Before `_main`, farrt zeroes the union, copies the resident initializers, then
   **maps each listed bank into the window and copies its slice** ROM → RAM.

Guard rails, all hard build errors: a `RESERVE` region colliding with the layout
(or with `SHADOW`); statics running into the system area (`0xF380`); a bank with
initialized statics whose RUN is not the `0x8000` window (farrt copies through
the bank-2 window); more banks with init data than the table holds.

`SHADOW`/`ADDR_BANK2` are single-sourced in bankpack and injected into
`farrt.asm` + the generated thunks at **link time** (`-g`), so the .asm files
cannot drift from the build. Worked example: `test/bankdata/`.

### FARTAB directive (bank→bank calls)

```
FARTAB <base_symbol> <slot0_sym> <slot1_sym> ...
```

Declares a resident **dispatch table**: `base_symbol` is a resident data label
(reserve one `.dw 0` per slot), and each slot names a function in some bank. After
all banks link, bankpack looks up each function's run-address (from the `.noi`
files) and patches the little-endian address into the table's ROM bytes. A resident
per-target thunk maps the segment and calls `(table[slot])` — so bank→bank calls
route through the table and no bank needs another bank's address at link time.

See `test/bankcall/` for a worked example: a C function in segment 5 calls a C
function in segment 6 (nested), verified on hardware.

## How it stays acyclic

The hard part of banking is the circular symbol dependency (resident calls banks,
banks call resident + each other). bankpack breaks it by rule:

1. Link the **resident** bank; record its exported symbols (from the full-name
   NoICE `.noi` — the `.map` truncates names at 9 chars).
2. For each other bank, scan its `.rel` for the externals it **references**
   (`S <name> Ref…`), look each up in the resident symbol table, and inject only
   those via `sdldz80 -g <name>=<addr>`.

So no bank ever needs another bank's address at link time. Cross-*bank* calls are
expected to route through a resident dispatcher (by ID), not by direct symbol —
only **resident** symbols are injectable here.

## Assembly

Each bank's `.ihx` is converted to raw binary with `sdobjcopy -I ihex -O binary`
(byte 0 = the run-address content) and `dd`'d into a `0xFF`-filled ROM at its
segment's file offset. A bank larger than 8 KB is a hard error.

## Runtime helpers (`*`, `/`, `long`, `float` in bank code)

The Z80 has no multiply/divide/long/float, so SDCC emits `call __mulint` (etc.) to
small library routines in `z80.lib`. bankpack handles these automatically: it scans
every bank for referenced `__*` helpers and forces them **once into the resident
bank** (page 1, always mapped), then resolves each bank's reference to that shared
copy via `-g`. So a helper is never duplicated per bank, and reaching it needs no
bank switch (a helper is a pure register/stack computation — mapper-independent).

Measured helper sizes (resident cost, one-time): `*` ~20 B, `u16 /` ~50 B,
`i16 /` ~110 B, `u32 *` ~280 B, `u32 /` ~150 B, `float` ~1.9 KB. Nothing to do —
`u16 a*b` in a banked C function just works (see `test/farturnkey`, bank 6 = `y*y`).

`z80.lib` is auto-located; override with `BANKPACK_Z80LIB=/path/to/z80.lib`.

## Safety checks (automatic, on every build with FAR lines)

After assembling the ROM, bankpack verifies the invariants banking relies on:
- each generated thunk `_far_<name>` is **resident** (below the 0x8000 window);
- each far target `_<name>` is **in the window** (>= 0x8000);
- (heuristic) a warning if resident code contains a direct `call`/`jp` into the
  window range `0x8000-0xBFFF` — such a branch bypasses a thunk and would jump
  into whichever segment happens to be mapped.

A hard failure aborts the build (`CHECK FAIL: ...`). Note that a plain
`foo()` instead of `far_foo()` to a bank function is *already* caught earlier as
an undefined-symbol link error, since bank symbols aren't injected into callers.

## Far-call cost (measured, `test/bankperf`)

Exact T-states on C-BIOS/openMSX, interrupts off: **a far-call costs ~170 T-states
(~47 us) over a near call** — so ~350 far-calls fill a 60 Hz frame. Bank *cold*
code (menus, level/entity logic, init): fine. Never bank a loop that iterates
thousands of times per frame. Same-bank calls are ordinary near calls (0 overhead).

## Far-call signature constraint

The generated thunk preserves `HL/DE/BC/IX` and clobbers only `A/F`, so far
functions must use **register-passed** args and returns: 16-bit / pointer / `void`
(these ride in `HL`/`DE`/`BC`). **Avoid `u8`/`bool`/`char` args or returns** (they
ride in `A`) and more than ~3 args (they spill to the stack). Use `u16`.

## Toolchain gotchas baked into this tool (so you don't rediscover them)

- **sdld's first file arg is the OUTPUT basename**, not an input — bankpack always
  passes an explicit output, then the inputs.
- **The `.map` truncates symbol names at 9 chars** — bankpack reads symbols from the
  full-name NoICE `.noi` (`sdld -j`).
- **`sdld -g sym=expr`** injects a symbol; injecting an *unreferenced* one errors,
  so only each bank's actual `Ref` externals are injected (same for `SHADOW`/
  `ADDR_BANK2`: only links that reference them get the `-g`).
- **`sdld -b _AREA=addr` for an area no input declares** also errors — the
  `_DATA` base is passed only when a module in the link declares `_DATA` (pure-asm
  residents don't).
- **Area order in a crt0-less link** follows the first module's declaration order,
  which parks `_INITIALIZER` in RAM — the generated `_bp_bankorder.rel` is linked
  first in every bank to pin the crt0 order instead.
- **On Windows, sdld writes CRLF `.noi`/`.map`** — the address arithmetic here
  would abort on the trailing `\r`. bankpack strips CR after every link.
- The `jp (ix)` helper is kept **inside** the generated thunk module (a cross-module
  reference to it from the runtime hit an sdld resolution quirk).

## Working examples

- `test/bank2/`      — asm banks; window switch + bank→resident call.
- `test/bankcall/`   — C banks; nested bank→bank call via a hand-written table.
- `test/farturnkey/` — **all C + manifest**; thunks/table/header generated.
- `test/bankdata/`   — **statics in every bank** (init data + BSS + `RESERVE`),
  asserted on C-BIOS_MSX1 and C-BIOS_MSX2.

## Not yet handled (roadmap)

- ASCII-16 / other segment sizes (currently 8 KB fixed).
- Unify the runtime shadow with `lib/ext/memseg` (farrt uses its own shadow byte,
  though its address is now the manifest's `SHADOW`).
