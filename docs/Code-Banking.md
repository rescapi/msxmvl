# Code Banking (`bankpack` + `farrt`)

Run **more code than fits in 64 KB** from an ASCII-8 MegaROM cartridge. Where [Far
Pointers](Far-Pointers.md) bank *data* in RAM, this banks *code* in ROM: functions live in
paged ROM segments and are called through a generated trampoline that maps the segment, calls
in, and restores. You write plain C plus a small manifest; `tools/bankpack.sh` generates the
glue and assembles the ROM.

> **This works on an MSX1 too.** The ASCII-8 mapper is **cartridge** hardware — the bank register
> lives on the cart, not in the machine — so banking needs nothing from the V9938 or the MSX2 RAM
> mapper. The turnkey banked-ROM test (`test/farturnkey`) is run on both C-BIOS_MSX2 and
> C-BIOS_MSX1 and produces identical results. This is the one way to get a lot of code onto an
> MSX1, which has no RAM mapper at all.

Full worked, tested example: **`test/farturnkey/`** (run `./run.sh` → `RESULT: PASS`). The
snippets below are that program.

## The idea in one picture

```
Z80 sees 64 KB:  [0000 BIOS][4000 resident code][8000 WINDOW][C000 RAM/stack]
                                    always mapped     ^ ASCII-8 pages a ROM
                                                        segment in here on demand
```

A **resident** bank (page 1, always mapped) holds `main`, the runtime, and the call
trampolines. Everything else lives in ROM **segments** that get paged into the `0x8000` window
only while a banked function runs. A cross-bank call costs ~170 T-states (~47 µs); a call
*within* the same bank is an ordinary near call (free). So group a subsystem into one bank and
pay the crossing once.

## What you write

**Banked source — ordinary C.** A function that lives in a bank, and one that calls into another
bank with the same `far_` syntax:

```c
// bankA.c  (lives in ROM segment 5)
#include "types.h"
#include "far.h"
u16 farA(u16 x) { return far_farB(x + 1) + 10; }   // calls into segment 6
```
```c
// bankB.c  (lives in ROM segment 6)
#include "types.h"
u16 farB(u16 y) { return y * y; }                  // uses a runtime multiply
```

**Resident `main` — call a banked function as `far_<name>()`:**

```c
// main.c  (resident)
#include "types.h"
#include "far.h"
volatile u8 __at(0xE000) R[8];
void main(void)
{
	u16 r = far_farA(5);           // resident -> bank 5 -> bank 6, all automatic
	R[0] = (r == 46) ? 0xA5 : 0;   // farA(5) = ((5+1)*(5+1)) + 10 = 46
}
```

**The manifest — which segment each bank and function lives in:**

```
# SEG  RUN     OBJ
  0    0x4000  farrt.rel main.rel      # resident: runtime + main (+ generated thunks)
  5    0x8000  bankA.rel               # farA
  6    0x8000  bankB.rel               # farB

FAR 5 u16 farA(u16 x)                  # declare far-callable functions + their bank
FAR 6 u16 farB(u16 y)
```

That is the entire developer-facing surface: `far_<name>(...)` at call sites, `<name>(...)`
defined normally in a bank, and one `FAR` line per entry point.

## Building

```sh
bankpack.sh --gen bank.manifest .        # generate far.h + the resident thunks/table
# compile C, assemble farrt.asm + the generated thunks
bankpack.sh bank.manifest game.rom 64    # link each bank, patch the table, write the ROM
```

`--gen` reads the `FAR` lines and emits **`far.h`** (the `far_<name>` declarations you include)
and the resident trampolines + dispatch table. The build step links every bank at its window
address and patches each function's real address into the table.

`test/farturnkey/run.sh` runs exactly this and asserts `farA(5) == 46` on emulated hardware.

## What bankpack handles for you

- **Acyclic linking.** No bank ever needs another bank's address at link time; cross-bank calls
  route through a resident dispatch table patched *after* all banks link.
- **Runtime helpers.** `*`, `/`, `long` in banked C pull SDCC helpers (`__mulint`, …); bankpack
  places them **once** in the resident bank and shares them — no per-bank duplication. (`farB`
  above uses `y * y` and just works.)
- **Safety checks.** Every build verifies each thunk is resident and each far target is in the
  window, and warns on a direct branch into banked space. Calling `foo()` instead of
  `far_foo()` is caught at link time as an undefined symbol.

## Signature rule for far functions

The trampoline preserves `HL/DE/BC/IX` and clobbers only `A/F`, so far functions must use
**register-passed** parameters and returns: `u16` / pointer / `void`. Avoid `u8`/`bool`/`char`
args and returns (they travel in `A`) and more than ~3 arguments (they spill to the stack).
Use `u16`. This is cheap on a Z80 anyway.

## Cost, measured

`test/bankperf/` measures it exactly: a far-call is **~170 T-states (~47 µs)** over a near call —
about **350 far-calls per 60 Hz frame**. Bank *cold*, coarse-grained code (menus, level logic,
init). Never bank a function called thousands of times per frame; keep hot inner loops resident.

## Reference

- `tools/README.bankpack.md` — manifest format, the `FARTAB` dispatch directive, and the
  toolchain gotchas the tool works around.
- `test/bank2/`, `test/bankcall/`, `test/farturnkey/`, `test/bankperf/` — progressively richer
  tested examples (asm → C → turnkey → cost).

## See also

- [Far Pointers](Far-Pointers.md) — bank *data* in RAM; often used together with banked code.
