# msxmvl Architecture & Optimization Methodology

## Goal
A best-in-class MSX2 C library: **fastest/closest-to-assembly**, modern-MSX capable (V9938, V9990,
MSX-DOS 1/2), well-documented — produced by an AI-driven benchmark + optimization loop. The API is a
clean-room reimplementation of MSXgl's (all ~796 functions across 35 modules), so existing MSXgl code
ports by recompilation.

## Clean-room discipline
msxmvl's implementations are written from **header contracts and documented hardware behavior only**.
MSXgl's `.c`/`.asm` sources are never read to author code — MSXgl is used purely as a *behavioral
oracle*: we run (or compile-and-diff) it to check that msxmvl produces identical results. This keeps
msxmvl free of MSXgl's CC BY-SA source lineage while guaranteeing behavioral compatibility.

## The verification arbiter: openMSX
Every function is **differentially tested** against the MSXgl original:
- `verify_rig.sh` — builds both libs, runs the same driver in openMSX, diffs an observable surface
  (VDP registers/VRAM/palette, PSG, V9990, RAM). Hardware truth, not assumptions.
- `verify_dos_static.sh` — for DOS wrappers (whose effects are hard to observe live), compiles
  msxmvl's AND MSXgl's real `dos.c` to Z80 asm and diffs each function's normalized instruction
  stream. This caught 40+ real bugs a naive same-source runtime rig reported as false passes.
- `verify_dos.sh` — boots real MSX-DOS 1.03 / 2.20 headless for end-to-end BDOS execution.
- Statuses tracked in `compat/COVERAGE.tsv`; `verified_divergent` marks functions proven correct
  where MSXgl is a bad oracle (buggy, uncompilable, or dead-linked).

## The performance bar
The library must be **faster and/or smaller on the majority of functions, and never slower** than the
baselines (MSXgl always; Fusion-C / z88dk where they implement the call). Two scorecards enforce it:
- `scorecard.py` — per-function **size** (bytes) vs MSXgl across all comparable modules → `SCORECARD_size.tsv`.
- `bench_call.sh` — per-function cycle-exact **speed** comparison (the decisive never-slower measure).
- `bench_batch.sh` — measures many functions per boot for fast triage (confirm decisions with `bench_call`).

## The optimization loop
For each function: measure size + cycles vs MSXgl → if bigger *and* slower, rewrite (usually tight
inline asm) → re-verify **all three axes** (byte-identical output, size, cycles) → commit, or revert
if it doesn't strictly improve. Nothing regresses: every commit is verified-or-reverted.

## The systemic finding
The dominant cause of C-vs-asm slowdown is SDCC bloating **variable shifts** (`1 << c`) and
**computed array indices** (`src[7-i]`) into shift-loops and index arithmetic. The fix pattern is
consistent: tight asm with fixed operations, pointer walks, `add hl,bc` power subtraction, LUTs, and
the `ldir`-overlap fill. Recognizing this makes the remaining optimization largely mechanical.

## Where msxmvl stands (honest)
- **Size:** ~24% smaller overall than MSXgl; smaller on ~114 functions, equal on ~48.
- **Speed:** wins the loop/string primitives; MSXgl's exhaustively hand-tuned hot modules
  (sprite_fx rotates, some memory fills) are still faster and are being closed function-by-function.
- **Verification:** ~736/796 functions differentially verified; the remainder are config-gated,
  device-only, or the clean-room crypt residue.

See `compat/REPORT_per_api_call.md` and `compat/REPORT_per_benchmark.md` for the live numbers.
