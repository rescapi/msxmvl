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
> turnkey test (`test/farturnkey`) runs on both C-BIOS_MSX1 and C-BIOS_MSX2 with identical results.

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

It looks like ordinary C. A function in one bank can call a function in another with a `far_`
prefix; the tool wires up the rest.

**A "gameplay" bank that calls a "scoring" bank:**

```c
// gameplay.c  (lives in ROM bank 5)
#include "types.h"
#include "far.h"

// Finish a level: award the level's points, plus a fixed clear bonus.
u16 play_level(u16 level)
{
	return far_level_score(level) + 50;   // 50-point clear bonus; scoring lives in bank 6
}
```
```c
// scoring.c  (lives in ROM bank 6)
#include "types.h"

// Base score for a level: 100 points per level number.
u16 level_score(u16 level)
{
	return level * 100;                   // a runtime multiply — see note below
}
```

**Resident `main` — call a banked function as `far_<name>()`:**

```c
// main.c  (resident, always mapped)
#include "types.h"
#include "far.h"
volatile u8 __at(0xE000) R[8];

void main(void)
{
	u16 score = far_play_level(3);         // resident -> gameplay bank -> scoring bank
	R[0] = (score == 350) ? 0xA5 : 0x00;   // play_level(3) = (3*100) + 50 = 350
}
```

**A one-line-per-bank manifest — which bank each function lives in:**

```
# SEG  RUN     OBJ
  0    0x4000  farrt.rel main.rel      # resident: runtime + main (+ generated thunks)
  5    0x8000  gameplay.rel            # play_level
  6    0x8000  scoring.rel             # level_score

FAR 5 u16 play_level(u16 level)        # declare each far-callable function + its bank
FAR 6 u16 level_score(u16 level)
```

That's the whole developer-facing surface: `far_<name>(...)` at the call site, `<name>(...)`
written normally inside a bank, and one `FAR` line per entry point.

## Building

```sh
bankpack.sh --gen bank.manifest .        # generate far.h + the resident thunks/table
# compile your C, assemble farrt.asm + the generated thunks
bankpack.sh bank.manifest game.rom 64    # link each bank, patch the table, write the ROM
```

`--gen` reads the `FAR` lines and writes **`far.h`** (the `far_<name>` declarations you `#include`)
plus the resident call thunks and dispatch table. The build step links every bank at the window
address and patches each function's real address into the table.

## What bankpack handles for you

- **No circular linking.** No bank ever needs another bank's address at link time — cross-bank
  calls route through a resident dispatch table that's patched *after* every bank has linked.
- **Shared runtime helpers.** Banked C that uses `*`, `/`, or `long` pulls in SDCC helpers like
  `__mulint`. bankpack places them **once** in the resident part and shares them, so `level_score`'s
  `level * 100` works with no per-bank duplication.
- **Mistake-proofing.** The build checks every thunk is resident and every far target is in the
  window. Forgetting the `far_` prefix (calling `play_level()` instead of `far_play_level()`) is
  caught at link time as an undefined symbol, not as a crash.

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

- A complete, tested banking ROM is in the source tree at **`test/farturnkey/`** — the snippets
  above are that program (it builds `game.rom` and asserts `play_level(3) == 350` on emulated
  hardware). `test/bank2`, `test/bankcall`, and `test/bankperf` are progressively richer examples
  (assembly → C → cost measurement).
- `tools/README.bankpack.md` — the manifest format and toolchain details.

## See also

- [Far Pointers](Far-Pointers.md) — bank *data* in RAM; games usually use both together.
