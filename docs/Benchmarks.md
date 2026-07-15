# Benchmarks

Every number on this page is **measured**, not estimated: the same driver program is linked once
against MSXgl and once against msxmvl, run in **openMSX** (a cycle-accurate emulator), and the Z80
T-states between two markers are counted. The emulator is the arbiter, not opinion.

## The scoreboard

152 functions, benchmarked head-to-head against MSXgl:

| Result | Count |
|---|---|
| **Faster** than MSXgl | **109** |
| Identical (within noise) | 41 |
| **Slower** | **1** |
| Divergent (different algorithm, not comparable) | 1 |

**Median speed-up on the functions that improved: 12%.** The single regression is
`Print_SetFont`, at **−2.8%** — a setup call that runs once per screen, which we accepted in exchange
for a faster `Print_DrawInt` on the hot path.

That is the honest headline: msxmvl is *usually* faster, *often* by a little, *sometimes* by a lot,
and — apart from one cold-path setup function — never meaningfully slower.

## Where the wins are

Representative results, in Z80 T-states (lower is better):

| Module | Function | msxmvl | MSXgl | Faster by |
|---|---|---|---|---|
| `v9990` | `V9_ClearVRAM` | 786,904 | 7,212,394 | **89%** |
| `string` | `String_Format` | 561,411 | 2,440,131 | **77%** |
| `print` | `Print_SetTextFont` | 1,235,299 | 4,830,463 | **74%** |
| `print` | `Print_SetColor` | 19,363 | 75,623 | **74%** |
| `print` | `Print_DrawInt` | 1,193,899 | 2,395,547 | **50%** |
| `vdp` | `VDP_SetAdjustOffset` | 5,091 | 10,275 | **51%** |
| `print` | `Print_SetBitmapFont` | 91,107 | 163,539 | **44%** |

The large wins are not micro-optimisation — they are **algorithmic**. `V9_ClearVRAM` and
`Print_SetTextFont` move bulk data, and the Z80's block instructions (`OTIR`, `LDIR`) beat a
per-byte loop by an order of magnitude when you can arrange to use them. `String_Format` avoids a
generic dispatch that costs more than the formatting itself.

The routine 5–15% wins across the rest come from the boring things: keeping values in registers,
not reloading constants inside loops, and picking the instruction that sets the flag you were about
to test anyway.

## How to read these numbers

**A word of caution about setup functions.** Some of the biggest ratios are on `*_Initialize` /
`*_Detect` calls, where the two libraries genuinely *do different amounts of work* — a cheaper
detection path is faster, but it is not the same function. Those are excluded from the table above.
Compare hot paths, not constructors.

**Cycle counts are not frame budgets.** A 60 Hz frame on a 3.58 MHz Z80 is about **59,600
T-states**. `Print_DrawInt` at 1.19M T-states is not "slow" — it is a benchmark looping the call
many times. What matters is the *ratio* between the two libraries on identical work.

## The rules we held ourselves to

The project's end condition was written down before the optimisation started, so it couldn't be
moved afterwards:

> Faster and/or smaller on the **majority** of benchmarks, and **never slower** than MSXgl on any of
> them.

We came one function short of the second half, and we say so rather than quietly dropping
`Print_SetFont` from the suite.

## Why this is even possible

Optimising against a moving target is guesswork. The MSX2 is not a moving target:

- **The cost model is exact.** Every Z80 instruction has a documented, fixed T-state count. There
  is no cache, no branch predictor, no out-of-order execution. "Faster" is a number you can compute,
  not a benchmark you have to trust.
- **There is a free correctness oracle.** MSXgl already exists. Any function can be checked for
  byte-identical output against it in the emulator, so speed work cannot silently break behaviour.
- **The hardware constraints are checkable.** The VDP's minimum VRAM access gap (15 T on a V9938,
  ~29 T on an MSX1's TMS9918) is a static rule — you can verify a loop respects it by reading the
  code, and *the tests catch it when you don't*. That rule is exactly how we found a real byte-drop
  bug on MSX1: see [VDP Access](VDP-Access.md).

That combination — exact costs, a free oracle, and statically checkable rules — is rare, and it is
what makes "measure everything" a practical policy here instead of an aspiration.

## Reproducing this

The benchmark harness lives in `compat/`. It fetches MSXgl, builds both libraries from the *same*
driver source, and runs them under openMSX:

```sh
compat/bench_batch.sh          # run the suite
cat compat/PERF_FRESH.tsv      # module, function, msxmvl cycles, msxgl cycles, verdict
```

## See also

- [Why msxmvl](Why-msxmvl.md) — what the project is for.
- [VDP Access](VDP-Access.md) — where the VRAM timing rules bite.
