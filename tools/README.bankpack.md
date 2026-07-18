# bankpack

Turns "banked code" into a real **MegaROM** — ASCII-8, ASCII-16, Konami, or Konami-SCC
mapper (see the mapper table below), images up to the mappers' 256-segment limit. Pure
bash + the SDCC toolchain (`sdldz80`, `sdasz80`, `sdobjcopy`) + coreutils. **No Python.**
Three modes:

```sh
bankpack.sh --gen   <manifest> [outdir]             # generate far.h + far thunks
bankpack.sh         <manifest> <out.rom> [rom_kb]   # link banks + assemble the ROM (rom_kb=64)
bankpack.sh --build <manifest> <out.rom> [rom_kb]   # ONE command: gen + compile + build
```

Runs in the **current directory**: reads the `.rel` files named in the manifest and
writes intermediates (`_bp_*.ihx/.map/.noi/.bin`, compiled `.rel`) there.

## Turnkey flow

The developer writes plain C + a manifest. A bank source marks each far-callable
**definition** with `FAR_FN`, and callers use the function's **own name**:

```c
FAR_FN u16 play_level(u16 level) { ... }   // in a bank source
...
score = play_level(3);                      // any caller, resident or another bank
```

then one command builds the ROM:

```sh
bankpack.sh --build bank.manifest game.rom 64
```

`--build` runs the whole pipeline: scan `FAR_FN` → generate `far.h` +
`_bp_far_thunks.asm` (written **next to the manifest**, so `#include "far.h"`
resolves from the sources), compile every manifest OBJ that has a sibling `.c`
(`sdcc -c -mz80 --sdcccall 1`) or `.asm` (`sdasz80`) next to the manifest, assemble
`farrt.asm` + the generated thunks, then link banks, patch the tables, and write the
ROM. An OBJ with no sibling source but an existing `.rel` is used as-is (prebuilt).

`farrt.asm` is auto-located by searching **upward from the manifest's directory** for
`lib/ext/farrt.asm`; a copy next to the manifest wins over the repo one, and the env
var `BANKPACK_FARRT=/path/to/farrt.asm` overrides both (needed when building in a
scratch dir outside the repo). When the upward search finds the repo, its `lib/gen`
is added to the compiler include path (for `types.h`).

The **two-step flow keeps working unchanged**, for build systems that own the
compile step:

1. `bankpack --gen bank.manifest .` → **`far.h`** (call declarations) and
   **`_bp_far_thunks.asm`** (resident thunks + dispatch table).
2. compile/assemble: `sdasz80` the runtime (`lib/ext/farrt.asm`) + `_bp_far_thunks.asm`;
   `sdcc -c` the banks and `main` (they `#include "far.h"`).
3. `bankpack bank.manifest game.rom` → link each bank, patch the dispatch table,
   assemble the ROM.

See `examples/banking/farturnkey/` for the all-C, one-command worked example.

### FAR_FN scanning rules

The gen step (and `--build`) derives far entries from the sources by **pure text
extraction** — no compile step:

- For every OBJ of every bank, `<name>.rel` maps to `<name>.c` **next to the
  manifest**; a missing `.c` is simply not scanned (explicit `FAR` lines cover it).
- A line whose **first token is `FAR_FN`** contributes one far function. The
  prototype is the rest of the line up to `{`, so the **full prototype must sit on
  the `FAR_FN` line**; the owning SEG is the bank whose OBJ list named the `.rel`.
- `far.h` `#define`s `FAR_FN` to nothing, so annotated sources compile normally.
- Explicit `FAR <seg> <proto>` manifest lines still work, and mix freely with
  `FAR_FN`. The **same function via both routes (or twice) is a build error**.
- `FAR_FN` in a **resident** source is a build error ("resident functions need no
  FAR_FN") — resident code is always mapped and needs no thunk.

### Natural call names + `far_` aliases

Each generated thunk is named after the far function **itself** (`_play_level`), so
callers just write `play_level(3)` — no prefix. This cannot collide with the bank's
own definition because banks link **separately**; correspondingly, the build resolves
each far target's address from the **owning bank's `.noi`** (the manifest says which
SEG the function lives in), never from a merged symbol map, and the safety checks do
the same. Every thunk also carries **`_far_<name>` as an alias label at the same
address** (zero cost), and `far.h` declares both prototypes — so existing
`far_<name>(...)` callers keep compiling and linking unchanged.

## Manifest

Whitespace-separated; `#` comments and blank lines ignored.

```
# SEG  RUN      OBJ [OBJ ...]
  0    0x4000   resident.rel          # <- first line = RESIDENT bank
  4    0x8000   physics.rel helpers.rel
  5    0x8000   render.rel
```

- **SEG** — segment number (8 KB each; 16 KB on ASCII-16); file offset =
  `SEG * segment size`.
- **RUN** — address the bank is *linked to run at* (`0x4000` resident; `0x8000`
  for the bank-2 window).
- **OBJ** — one or more `.rel` files linked into that bank.

The **first line is the resident bank**: linked first, and the only bank that may
be referenced by name from the others.

Far functions are declared either with `FAR_FN` in the bank source (preferred, see
the scanning rules above) or explicitly in the manifest:

```
FAR 5 u16 play_level(u16 level)     # SEG + C prototype
```

Data-model keywords (all optional):

```
RAMBASE 0xC000          # statics RAM base (default 0xC000)
RESERVE 0xC800 0xC880   # program-owned RAM [lo,hi): build FAILS on collision
SHADOW  0xE020          # memseg shadow byte (env BANKPACK_SHADOW overrides)
MAPPER  ASCII16         # cartridge mapper (default ASCII8)
```

### Mappers

| `MAPPER`     | window select | segment | resident (0x4000)          | openMSX `-romtype` |
|--------------|---------------|---------|----------------------------|--------------------|
| `ASCII8`     | `0x7000`      | 8 KB    | SEG 0 (reset state)        | `ASCII8`           |
| `ASCII16`    | `0x7000`      | 16 KB   | SEG 0 (reset state)        | `ASCII16`          |
| `KONAMI`     | `0x8000`      | 8 KB    | SEG 0 (**hardware-fixed**) | `Konami`           |
| `KONAMI_SCC` | `0x9000`      | 8 KB    | SEG 0 (reset state)        | `KonamiSCC`        |

Every supported mapper selects the `0x8000` window with **one 8-bit register
write**, so `farrt.asm`, the generated thunks, the far-call cost, and the whole
data model are identical across all of them — a mapper swap costs the program one
manifest line. Per mapper: `ASCII16` switches to **16 KB segments** (file offset
`SEG * 0x4000`; a single bank may carry up to 16 KB of code + const data); the
`KONAMI`/`KONAMI_SCC` select registers sit *inside* the window (harmless — the
mapper eats the write, ROM reads are unaffected); on `KONAMI` the resident's 8 KB
at `0x4000` is hardware-wired to segment 0 — no select register exists for it.
Boot with the listed `-romtype` — always pass it explicitly, openMSX's
auto-detection guesses on homebrew images; the build's final report line prints
the right one.

### Big ROMs

Nothing structural caps the image at 64 KB: `rom_kb` scales the file and the
segment offsets scale with it, up to the 8-bit select register's 256 segments
(2 MB ASCII-8/Konami, 4 MB ASCII-16). One capacity to know about: `_bp_datatab`
holds 32 entries, which caps banks **with initialized statics** — not banks
overall. Worked example: `examples/banking/bankbig/` (512 KB ASCII-8, far code + statics in
SEG 63, the image's last segment).

### Manifest build checks (mapper + geometry)

All hard build errors:

- the **resident must be SEG 0** — every mapper's reset state maps segment 0 at
  `0x4000`, and on `KONAMI` it is hardware-fixed there (no register exists);
- every **SEG ≤ 255** — the mappers' select register is 8-bit;
- every **SEG inside the image** (`SEG < rom_kb*1024 / segsize`) — a bank past
  the end would otherwise silently **grow** the file via `dd seek` into an image
  no loader accepts;
- **`rom_kb` a multiple of the segment size**;
- `KONAMI_SCC` only: a code bank on a segment whose select value has the **low 6
  bits all 1** (`0x3F`/`0x7F`/`0xBF`/`0xFF`) is refused — writing such a value
  enables the **SCC sound chip** at `0x9800` instead of ROM, so a thunk mapping
  that segment would call into SCC registers, not code.

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
cannot drift from the build. Worked example: `examples/banking/bankdata/`.

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

See `examples/banking/bankcall/` for a worked example: a C function in segment 5 calls a C
function in segment 6 (nested), verified on hardware.

## How it stays acyclic

The hard part of banking is the circular symbol dependency (resident calls banks,
banks call resident + each other). bankpack breaks it by rule:

1. Link the **resident** bank; record its exported symbols (from the full-name
   NoICE `.noi` — the `.map` truncates names at 9 chars).
2. For each other bank, scan its `.rel` for the externals it **references**
   (`S <name> Ref…`), look each up in the resident symbol table, and inject only
   those via `sdldz80 -g <name>=<addr>`. A symbol the bank **defines itself** is
   never injected (with natural-name thunks the resident exports `_<name>` too;
   injecting it into the owning bank would be a duplicate definition — and an
   intra-bank call to a far function correctly stays a near call).

So no bank ever needs another bank's address at link time. A bank→bank call is a
reference to the resident *thunk* (which carries the far function's name), resolved
by the same injection — only **resident** symbols are injectable here. Far *target*
addresses, patched into the dispatch table after all banks link, are read from each
function's **owning bank's `.noi`** (the manifest's SEG says which), never from a
merged map — both the resident (thunk) and the owning bank (real code) define
`_<name>`, so a merged map would be ambiguous.

## Assembly

Each bank's `.ihx` is converted to raw binary with `sdobjcopy -I ihex -O binary`
(byte 0 = the run-address content) and `dd`'d into a `0xFF`-filled ROM at its
segment's file offset. A bank larger than the segment size (8 KB ASCII-8,
16 KB ASCII-16) is a hard error.

## Runtime helpers (`*`, `/`, `long`, `float` in bank code)

The Z80 has no multiply/divide/long/float, so SDCC emits `call __mulint` (etc.) to
small library routines in `z80.lib`. bankpack handles these automatically: it scans
every bank for referenced `__*` helpers and forces them **once into the resident
bank** (page 1, always mapped), then resolves each bank's reference to that shared
copy via `-g`. So a helper is never duplicated per bank, and reaching it needs no
bank switch (a helper is a pure register/stack computation — mapper-independent).

Measured helper sizes (resident cost, one-time): `*` ~20 B, `u16 /` ~50 B,
`i16 /` ~110 B, `u32 *` ~280 B, `u32 /` ~150 B, `float` ~1.9 KB. Nothing to do —
`u16 a*b` in a banked C function just works (see `examples/banking/farturnkey`, bank 6 = `y*y`).

`z80.lib` is auto-located; override with `BANKPACK_Z80LIB=/path/to/z80.lib`.

## Safety checks (automatic, on every build with far entries)

After assembling the ROM, bankpack verifies the invariants banking relies on:
- each generated thunk `_<name>` **and its `_far_<name>` alias** are **resident**
  (below the 0x8000 window) — looked up in the resident's `.noi`;
- each far target `_<name>` is **in the window** (>= 0x8000) — looked up in its
  **owning bank's** `.noi` (per the manifest's SEG), never a merged map;
- (heuristic) a warning if resident code contains a direct `call`/`jp` into the
  window range `0x8000-0xBFFF` — such a branch bypasses a thunk and would jump
  into whichever segment happens to be mapped.

A hard failure aborts the build (`CHECK FAIL: ...`). Note that calling a bank
function that was never marked `FAR_FN` (nor declared `FAR`) is *already* caught
earlier as an undefined-symbol link error, since bank symbols aren't injected
into callers.

## Far-call cost (measured, `examples/banking/bankperf`)

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

- `examples/banking/bank2/`      — asm banks; window switch + bank→resident call.
- `examples/banking/bankcall/`   — C banks; nested bank→bank call via a hand-written table.
- `examples/banking/farturnkey/` — **all C + manifest, one `--build` command**; `FAR_FN`
  annotations, natural-name calls, thunks/table/header generated; also asserts
  the FAR_FN-in-resident build error.
- `examples/banking/bankdata/`   — **statics in every bank** (init data + BSS + `RESERVE`),
  asserted on C-BIOS_MSX1 and C-BIOS_MSX2.
- `examples/banking/bank16/`     — **ASCII-16**: 16 KB segments, a bank with a checksummed
  **>8 KB payload**, the statics model intact, on both machines.
- `examples/banking/bankbig/`    — **512 KB ASCII-8** (64 segments): far code + statics in
  **SEG 63**, exact ROM file size, and SEG-beyond-ROM asserted to fail.
- `examples/banking/bankscc/`    — **KONAMI_SCC**: the turnkey program as a Konami-SCC
  image, plus the SCC seg-63 refusal asserted.
- `examples/banking/bankkonami/` — **KONAMI**: same program as a plain Konami image, plus
  the resident-must-be-SEG-0 refusal asserted.

## Not yet handled (roadmap)

- Unify the runtime shadow with `lib/ext/memseg` (farrt uses its own shadow byte,
  though its address is now the manifest's `SHADOW`).
