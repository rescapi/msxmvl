# Code Banking (`bankpack` + `farrt`)

The other half of the "bigger than 64 KB" story. [Far Pointers](Far-Pointers.md) let you store
more *data* than fits in memory; **code banking lets you run more *code*** than fits — a game with
far more logic (menus, cutscenes, level scripts, enemy AI, an ending sequence) than 64 KB of Z80
address space could ever hold at once.

You write plain C. Functions live in paged ROM banks and you call them like any other function;
the library maps the right bank in, runs the code, and maps it back out — automatically.

> **Works on MSX1 too.** Banking here uses an **ASCII-8 MegaROM cartridge**: the bank-select
> register is on the *cartridge*, not in the machine, so it needs nothing from the MSX2 hardware.
> This is the one way to get a lot of code onto an MSX1, which has no RAM mapper at all. The
> turnkey test (`examples/banking/farturnkey`) runs on both C-BIOS_MSX1 and C-BIOS_MSX2 with identical results.

## The idea in one picture

```
Z80 sees 64 KB:  [0000 BIOS][4000 resident code][8000 WINDOW][C000 RAM/stack]
                                    always mapped     ^ a ROM bank is paged
                                                        in here on demand
```

A small **resident** part (always mapped) holds `main`, the runtime, and the tiny "call thunks".
Everything else lives in ROM **banks** that are paged into the `0x8000` window only while one of
their functions runs. Calling into another bank costs about **170 T-states (~47 µs)** — so group a
whole subsystem into one bank and you pay that cost once at the door, not on every internal call. A
call *within* the same bank is a normal, free near-call.

## What you write

It looks like ordinary C. Mark a bank function callable from outside its bank with **`FAR_FN`**,
then call it from anywhere — resident or another bank — **by its own name**; the tool wires up
the rest.

**A "gameplay" bank that calls a "scoring" bank:**

```c
// gameplay.c  (lives in ROM bank 5)
#include "types.h"
#include "far.h"

// Finish a level: award the level's points, plus a fixed clear bonus.
FAR_FN u16 play_level(u16 level)
{
	return level_score(level) + 50;   // 50-point clear bonus; scoring lives in bank 6
}
```
```c
// scoring.c  (lives in ROM bank 6)
#include "types.h"
#include "far.h"

// Base score for a level: 100 points per level number.
FAR_FN u16 level_score(u16 level)
{
	return level * 100;               // a runtime multiply — see note below
}
```

**Resident `main` — call a banked function like any other function:**

```c
// main.c  (resident, always mapped)
#include "types.h"
#include "far.h"

void main(void)
{
	u16 score = play_level(3);   // resident -> gameplay bank -> scoring bank, automatic
	// score == 350:  play_level(3) = level_score(3) + 50 = (3 * 100) + 50
	...
}
```

**A one-line-per-bank manifest — which objects live in which bank:**

```
# SEG  RUN     OBJ
  0    0x4000  farrt.rel main.rel      # resident: runtime + main (+ generated thunks)
  5    0x8000  gameplay.rel            # FAR_FN play_level
  6    0x8000  scoring.rel             # FAR_FN level_score
```

That's the whole developer-facing surface: `FAR_FN` on the definition, a plain `name(...)`
at every call site, and one manifest line per bank. `FAR_FN` expands to nothing — the
build's text scan is what reads it — and the bank a function lives in follows from which
OBJ list names its `.rel`.

> **Compatibility:** the older spelling keeps working. A manifest can declare entry points
> explicitly (`FAR 5 u16 play_level(u16 level)`) instead of — or alongside — `FAR_FN`, and
> every far function is also callable as `far_<name>(...)`: the generated `far.h` declares
> both names, and the thunk carries both labels at the same address (zero cost).

## Building

One command builds the ROM — scan `FAR_FN`, generate `far.h` + the thunks, compile every
source, link each bank, patch the tables, write the image:

```sh
bankpack.sh --build bank.manifest game.rom 64
```

`--build` compiles each manifest OBJ from its sibling `.c`/`.asm` next to the manifest,
locates `farrt.asm` by searching upward from the manifest for `lib/ext/farrt.asm`
(override with `BANKPACK_FARRT=/path/to/farrt.asm` when building outside the repo), and
adds the repo's `lib/gen` to the include path when it finds it.

The explicit two-step flow keeps working, for build systems that want to own the compile
step:

```sh
bankpack.sh --gen bank.manifest .        # generate far.h + the resident thunks/table
# compile your C, assemble farrt.asm + the generated thunks
bankpack.sh bank.manifest game.rom 64    # link each bank, patch the table, write the ROM
```

The gen step reads the `FAR_FN` annotations (and any explicit `FAR` lines) and writes
**`far.h`** (the declarations you `#include` — natural and `far_` names) plus the resident
call thunks and dispatch table. The build step links every bank at the window address and
patches each function's real address — read from its *owning bank's* link — into the table.

## What bankpack handles for you

- **No circular linking.** No bank ever needs another bank's address at link time — cross-bank
  calls route through a resident dispatch table that's patched *after* every bank has linked.
- **Shared runtime helpers.** Banked C that uses `*`, `/`, or `long` pulls in SDCC helpers like
  `__mulint`. bankpack places them **once** in the resident part and shares them, so `level_score`'s
  `level * 100` works with no per-bank duplication.
- **Mistake-proofing.** The build checks every thunk is resident and every far target is in the
  window. Calling a bank function you *forgot* to mark `FAR_FN` (or declare with a `FAR` line)
  is caught at link time as an undefined symbol, not as a crash — no thunk exists, and bank
  symbols are never injected into other banks' links. `FAR_FN` on a *resident* function is a
  hard build error too (resident code is always mapped; it needs no thunk).

## Statics in banked code — the data model

Banked C is not restricted to pure functions: a bank can have ordinary **writable statics**,
both initialized and zeroed, and they behave exactly as C promises:

```c
// bankA.c  (lives in ROM bank 5)
static u16 a_magic = 0xA55A;   // initialized: farrt must copy this bank's DATA slice
static u8  a_bss[16];          // BSS: farrt must zero it
```

Two things make that work, both automatic:

- **bankpack lays every bank's statics out in disjoint RAM.** The resident's `_DATA` links at
  `RAMBASE` (default `0xC000`); each bank then links with an explicit data base in the next free
  slice, so resident and bank statics can never overlap each other. The build prints the layout:

  ```
  bankpack: statics: resident  [0xc000,0xc00a)
  bankpack: statics: seg 5     [0xc00a,0xc01e)
  bankpack: statics: seg 6     [0xc01e,0xc032)
  ```

- **farrt runs the crt0 contract before `main`,** driven by a table bankpack patches into the
  ROM: zero the whole statics region, copy the resident's initial values, then **map each bank
  into the window and copy its initializer slice** ROM → RAM.

If your program owns RAM directly (`__at` buffers, a dynamic-memory arena), declare it in the
manifest and the build **fails on collision** instead of silently corrupting it:

```
RAMBASE 0xC000
RESERVE 0xC800 0xC880
```

The memseg shadow byte can be moved the same way (`SHADOW 0xE0F0`, or env `BANKPACK_SHADOW`)
— the value is injected into `farrt.asm` and the generated thunks at link time, so there is a
single source of truth. The complete tested program for all of this is **`examples/banking/bankdata/`**:
resident + two banks, each with initialized statics and BSS, asserted on C-BIOS_MSX1 *and*
C-BIOS_MSX2, plus a deliberate `RESERVE` collision asserted to fail the build.

## Other cartridges: one manifest line per mapper

The default build is an ASCII-8 MegaROM, but one manifest line retargets the whole
program to any of the supported cartridge mappers:

```
MAPPER ASCII16
```

| `MAPPER`     | segment | why you'd pick it                                     | openMSX `-romtype` |
|--------------|---------|-------------------------------------------------------|--------------------|
| `ASCII8`     | 8 KB    | the default; common flash/homebrew mapper             | `ASCII8`           |
| `ASCII16`    | 16 KB   | a subsystem or const table too big for an 8 KB bank   | `ASCII16`          |
| `KONAMI`     | 8 KB    | the classic Konami mapper; widest loader recognition  | `Konami`           |
| `KONAMI_SCC` | 8 KB    | runs **natively** on Carnivore2 / MegaFlashROM / Yamanooto flash carts | `KonamiSCC` |

Nothing else changes: every supported mapper selects the window through one cartridge
register write, so the runtime, the thunks, the call cost, and the statics data model are
identical — boot the image with the listed `-romtype` (the build prints it). Mapper quirks
are enforced as build errors, not left to crash on hardware: the resident must be segment 0
(hardware-fixed on `KONAMI`), and on `KONAMI_SCC` a code bank can't sit on a segment whose
select value would enable the SCC sound chip instead of ROM (63, 127, 191, 255).

**Bigger than 64 KB** is just a build argument — `bankpack.sh --build game.manifest
game.rom 512` writes a 512 KB image (up to 2 MB ASCII-8/Konami, 4 MB ASCII-16), and a bank
placed beyond the image is a build error. Tested programs: **`examples/banking/bank16/`** — an ASCII-16
bank carrying a checksummed **~9 KB const table** (impossible under ASCII-8);
**`examples/banking/bankbig/`** — a **512 KB** ASCII-8 image with far code + statics in segment 63;
**`examples/banking/bankscc/`** and **`examples/banking/bankkonami/`** — the turnkey program as Konami-SCC and
plain Konami images; all verified on C-BIOS_MSX1 and MSX2.

## One rule for far functions

The call thunk keeps your registers safe by passing arguments in registers, so far-callable
functions should use **`u16`, pointer, or `void`** parameters and returns, and no more than ~3
arguments. Prefer `u16` — it's the natural, cheap choice on a Z80 anyway. (Avoid `u8`/`char`/`bool`
in far signatures; those travel differently and the thunk doesn't carry them.)

## The cost, measured

A cross-bank call is **~170 T-states (~47 µs)** more than a near call — roughly **350 far-calls per
60 Hz frame**. So bank *coarse, cold* code: menus, level setup, cutscenes, AI decisions. Keep a hot
inner loop (something called thousands of times per frame) resident, or inside a single bank.

## Reference

- A complete, tested banking ROM is in the source tree at **`examples/banking/farturnkey/`** — the snippets
  above are that program (it builds `game.rom` and asserts `play_level(3) == 350` on emulated
  hardware). `examples/banking/bank2`, `examples/banking/bankcall`, and `examples/banking/bankperf` are progressively richer examples
  (assembly → C → cost measurement), and **`examples/banking/bankdata/`** is the statics data-model
  acceptance test (the snippets in the statics section are that program).
- `tools/README.bankpack.md` — the manifest format and toolchain details.

## See also

- [Far Pointers](Far-Pointers.md) — bank *data* in RAM; games usually use both together.
