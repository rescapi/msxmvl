# Proposal: ROM-mapper breadth for bankpack (Konami/SCC + big ROMs)

**Status:** IMPLEMENTED — `tools/bankpack.sh` (per-mapper table: ASCII8/ASCII16/KONAMI/
KONAMI_SCC + the mapper/geometry build checks below; `farrt.asm` unchanged, as designed),
acceptance tests `examples/banking/bankbig/` (512 KB ASCII8, statics in SEG 63), `examples/banking/bankscc/` and
`examples/banking/bankkonami/` (all green on C-BIOS_MSX1 + MSX2, negative cases asserted to fail), docs
updated (`Code-Banking.md`, `tools/README.bankpack.md`). The public tree picks docs/tools up
on the next make_public run (`test/` stays private, as usual). Originally demand-driven:
msxzork needs **big ASCII8**; flash-cart users (Carnivore2, MegaFlashROM, Yamanooto)
natively run **Konami-SCC** images. We do not chase MSXgl parity for its own sake.

## Why

bankpack's whole mapper story is one link-time value: `ADDR_BANK2` (`-g`,
single-sourced in the script) selects the 0x8000 window; `SEGSIZE` drives file
layout. Every mapper below keeps that shape — an 8-bit segment write to one
register — so `farrt.asm`, the generated thunks, and the data model carry over
**unchanged**. What differs per mapper is the register address, the fixed/reset
bank behavior, and hardware quirks that must become **build checks**.

## Per-mapper analysis

| MAPPER       | window select | SEGSIZE | resident (4000-5FFF)      | openMSX -romtype |
|--------------|---------------|---------|----------------------------|------------------|
| ASCII8       | 0x7000        | 8 KB    | seg 0 (reset state)        | ASCII8           |
| ASCII16      | 0x7000        | 16 KB   | seg 0 (reset state)        | ASCII16          |
| KONAMI       | 0x8000        | 8 KB    | seg 0 (**hardware-fixed**) | Konami           |
| KONAMI_SCC   | 0x9000        | 8 KB    | seg 0 (reset state)        | KonamiSCC        |

- **KONAMI (plain / "Konami4").** 4000-5FFF is hard-wired to segment 0 — no
  register exists for it. That matches our resident placement exactly (SEG 0,
  RUN 0x4000, ≤ 8 KB), but must become a BUILD CHECK: resident line is SEG 0 or
  die. (Make that check mapper-wide — every mapper's reset state already assumes
  it implicitly.) The select register sits **inside the window** (writes in
  0x8000-0x9FFF switch that page): fine — the mapper eats the write, ROM reads
  are unaffected, and farrt/thunks only ever write it. Other banks reset to
  seg = bank index on openMSX; we rely on nothing but the fixed resident —
  farrt INIT writes HOME(0) to the window register before first use.
- **KONAMI_SCC ("Konami5").** All four banks switchable; select for 0x8000 is
  0x9000 (decoded in 0x9000-0x97FF), also inside the window. Quirk: writing a
  value whose **low 6 bits are all 1** (0x3F, 0x7F, 0xBF, 0xFF) to the 0x9000
  register maps the **SCC sound chip** at 9800-9FFF instead of ROM. BUILD
  CHECK: refuse any manifest SEG with `(seg & 0x3F) == 0x3F` for code banks
  (a thunk mapping seg 63 would call into SCC registers, not code). Resident
  seg 0 at 4000 is reset-guaranteed (SCC chip reset), covered by the same
  resident-SEG-0 check.
- **Bigger ASCII8/16 (256 KB - 2/4 MB).** Not a new mapper — the 8-bit select
  register already addresses 256 segments (2 MB ASCII8, 4 MB ASCII16); see
  Big-ROM support below.

**Evaluated, deferred** (revisit on demand): **NEO8/NEO16** — 16-bit segment
values (two-byte select protocol) break the shared single 8-bit-write thunk;
hardware barely exists. **ASCII16X** — 16-bit segments for >4 MB flash; no
demand. **Yamanooto** — natively runs KONAMI_SCC images; its extra
ENAR/OFFR/CFGR registers matter only for flashing/offset tricks, so KONAMI_SCC
support IS Yamanooto support for us. **Popolon** — same select addresses as
KONAMI_SCC plus offset registers for >2 MB; the KONAMI_SCC subset covers it.

## What changes in bankpack.sh — and what does not

The `MAPPER` case in `parse_manifest` becomes a small table per mapper:
`ADDR_BANK2` + `SEGSIZE` + a check hook, and the final report line prints the
`-romtype` name. New build checks: resident SEG must be 0 (all mappers); the
SCC 0x3F-pattern refusal (KONAMI_SCC); every SEG < `ROM_KB*1024/SEGSIZE`
(today `dd seek` past the image end silently grows the file); SEG ≤ 255
(8-bit register, all planned mappers); ROM_KB a multiple of SEGSIZE/1024.

**Unchanged, verified per mapper:** `lib/ext/farrt.asm` and the generated
thunks (ADDR_BANK2/SHADOW arrive at link time via `-g`; the 8-bit
`ld a,#seg / ld (ADDR_BANK2),a` sequence is valid for all four mappers — only
farrt's *comments* name 0x7000), the statics data model, disjoint-RAM layout,
and the window safety checks (thunk resident / target ≥ 0x8000). **One
exception:** raising `DATATAB_MAX` (see below) also grows `_bp_datatab`'s
`.ds` in farrt.asm — the two constants must move together.

## Big-ROM support: what actually limits us today

Nothing structural: `ROM_KB` is already a parameter and segment file offsets
scale. The real limits: (a) `DATATAB_MAX=32` caps banks **with initialized
statics** (not banks overall) — keep, document, raise only when a program
hits it; (b) 8-bit seg fields in thunks + datatab cap segments at 255 — same
as the mappers' own registers, so a check, not a redesign; (c) **nothing above
64 KB is tested**, and a SEG beyond the image is silently accepted; (d) always
pass `-romtype` to openMSX — auto-detection guesses on homebrew images.
Acceptance: a **512 KB ASCII8** build (64 segments) with a far bank + statics
at **SEG 63**, asserted end-to-end.

## Acceptance tests

House pattern (`examples/banking/bank16/run.sh`): scratch dir, `--gen` + sdcc, openMSX
breakpoint at `_mark_end`, result bytes at 0xE000, asserted on C-BIOS_MSX1
**and** MSX2, plus negative build-check cases.

- `examples/banking/bankbig/` — 512 KB ASCII8, far call + statics in SEG 63, ROM file size
  asserted; negative: SEG 64 refused ("beyond ROM size").
- `examples/banking/bankkonami/` — farturnkey-shaped program, `MAPPER KONAMI`,
  `-romtype Konami`; negative: resident on SEG 1 refused.
- `examples/banking/bankscc/` — same program, `MAPPER KONAMI_SCC`, `-romtype KonamiSCC`;
  negative: a bank on SEG 63 refused (message names the SCC quirk).

## Priority & effort (vs demand)

1. **Big-ROM ASCII8** — S (checks + test; likely zero mapper code). Direct
   msxzork demand.
2. **KONAMI_SCC** — M (mapper table + 0x3F check + test). Flash-cart users;
   later unlocks SCC audio alongside code banking.
3. **KONAMI plain** — S once the table exists. Widest loader recognition;
   nearly free.
4. Everything else: deferred (reasons above), one table row each when demand
   appears.

## Docs to update (on implementation)

`Code-Banking.md` (mapper section → table + romtype per mapper),
`tools/README.bankpack.md` (MAPPER values, new build checks, big-ROM notes),
this doc's Status. The public msxmvl tree picks docs/tools up on the next
make_public run; `test/` stays private as usual.

## Then, downstream

msxzork moves to a 512 KB ASCII8 image (story data + more code banks);
flash-cart users build KONAMI_SCC images that run unmodified on Carnivore2,
MegaFlashROM, and Yamanooto.
