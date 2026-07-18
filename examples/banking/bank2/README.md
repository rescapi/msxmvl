# bank2 — ASCII-8 far-call proof

Smallest possible end-to-end proof that **banked code works** on this stack:
a 2-bank ASCII-8 MegaROM where a routine that lives *only* in a paged-out ROM
segment is called from the resident bank, executes, and the previous segment is
restored — verified on real emulated hardware (C-BIOS_MSX2 under openMSX).

This de-risks the `lib/ext` code-banking design (`FAR_FN`/trampoline + the
`bankpack` build step) before any of it is written.

## What it demonstrates

1. **`bankpack` end-to-end** (`tools/bankpack.sh`, driven by `bank.manifest`):
   each bank is linked to *run* at its window address, then placed at the *file*
   offset of its assigned 8 KB segment.
     - `resident.asm` → segment 0, runs at `0x4000` (cart header + INIT).
     - `banked.asm`   → segment 4, runs at `0x8000` (the banked routine).
2. **Slot selection**: C-BIOS calls INIT with only page 1 (`0x4000`) on the cart
   slot; the resident code copies that slot into page 2 (`0x8000`, port `0xA8`)
   so the bank-2 window is visible. *(Non-expanded-slot path — see caveat.)*
3. **The far-call**: write the segment number to `0x7000` (ASCII-8 bank-2
   select), `call 0x8001`, then restore.
4. **A bank→resident call**: the banked routine calls `_res_helper` (in the
   resident bank) *by name*. `bankpack` resolves that symbol's address from the
   resident link and injects it (sdld `-g`) into the bank's link — the acyclic
   cross-bank linkage the design relies on.

## Result bytes (RAM `0xE000`)

| byte | meaning | pass value |
|------|---------|-----------|
| RES[0] | overall verdict | `A5` |
| RES[1] | window byte **before** mapping (reset seg 0) | `41` (`'A'`) |
| RES[2] | window byte **after** mapping (banked marker) | `B4` |
| RES[3] | value the **banked code** wrote | `5A` |
| RES[4] | window byte **after** restore (seg 0 again) | `41` (`'A'`) |
| RES[5] | value written by `_res_helper` (**bank→resident call**) | `3C` |

## Run

```sh
./run.sh      # assemble → bankpack (link per bank + assemble ROM) → boot → assert
```

Expected tail: `RESULT: PASS`.

## Caveats (intentionally out of scope for the proof)

- **Slot selection is the non-expanded-slot path** (direct port `0xA8`). A real
  `memseg` must use `ENASLT` / secondary-slot registers to handle expanded slots.
- **Previous segment is assumed 0** (reset state) rather than read from a shadow;
  the real trampoline reads it from the `memseg` RAM shadow so nested far-calls
  restore correctly.
- No `di`/`ei` around the switch (single-threaded proof, no ISR touching page 2).

These three are exactly what the `memseg` core + `Far_With` API add on top.
