# Proposal: a C-runtime data model for bankpack (`farrt` globals-init + per-bank data)

**Status:** IMPLEMENTED — `tools/bankpack.sh` (A/C/D) + `lib/ext/farrt.asm` (B), acceptance
test `test/bankdata/` (green on C-BIOS_MSX1 + MSX2, collision case asserted to fail), docs
updated (`Code-Banking.md`, `tools/README.bankpack.md`). The public tree picks the lib/tools/docs
changes up on the next make_public run (the acceptance test stays private, like all of `test/`).
Originally motivated by the msxzork ASCII-8 port, which bankpack couldn't build before this.

## Why

`bankpack` + `farrt` today assume banked code has **no writable C statics**. That
holds for `test/farturnkey` (pure register functions), but not for a real program.
The msxzork Z-machine interpreter is full of statics (VM call-frame stack, screen
buffers, dictionary token arrays, platform hook pointers), and hit three walls:

1. **`farrt` does no globals-init.** `crt0_rom16` copies initialized data ROM→RAM
   and zeroes BSS (`INIT_GLOBALS`); `farrt` does not (no `ldir`/gsinit). So a banked
   build gets **un-zeroed BSS and un-copied initialized globals**.
2. **No per-bank DATA/BSS coordination.** `link_bank` passes only `-b _CODE=<run>`;
   each bank is linked separately, so every bank's statics start at the same RAM
   address and **overlap** (resident vs banked, and bank vs bank).
3. **`SHADOW` is hardcoded (0xE020)** and can land inside program data (msxzork's
   dynamic memory is `__at(0xC000)`, spanning 0xE020). It must be relocatable.

## What to build

### A. Coordinated DATA/BSS layout (`bankpack.sh`)
- Link the resident bank; record its `_DATA`/`_BSS` extents from the `.noi`.
- Link each later bank with explicit `-b _DATA=<addr> -b _BSS=<addr>` placing its
  statics in a **disjoint** RAM sub-range (after the resident, then bank-by-bank).
- Emit a table the runtime can read: for each bank, its RAM DATA dest, length, and
  ROM initializer segment+offset; plus the BSS union `[lo,hi)`.
- Add a **reserved-region** input so a program with `__at` data (msxzork's dynmem
  `[0xC000,0xEC12)`) declares it off-limits and gets a **build error** on collision
  rather than silent corruption.

### B. Globals-init (`farrt.asm`)
- Before `_main`: zero the whole BSS union, then for each bank, **map it** (via
  `ADDR_BANK2`), `ldir` its initialized DATA slice ROM→RAM, and restore. (Resident
  init is the ordinary crt0 copy/zero.)

### C. Configurable SHADOW (`bankpack.sh` + `farrt.asm`, kept in sync)
- Make `SHADOW` a parameter (env/flag) or place it in the coordinated data region,
  so it never overlaps program RAM.

### D. Windows CRLF safety (already found; fold in)
- sdld writes CRLF `.noi`/`.map` on Windows; `bankpack`'s arithmetic on parsed
  addresses then aborts the FARTAB patch. Strip CR after each `sdldz80` link
  (`sed -i 's/\r$//' "$base.noi" "$base.map"`). (msxzork vendors this patch today.)

## Acceptance test (`test/bankdata`, all-C)
- Resident + a banked module **each with BSS and initialized statics**, plus an
  `__at` reserved region. Assert on emulated MSX (C-BIOS_MSX1 **and** MSX2, like
  `farturnkey`) that every static holds its correct initial value and that
  resident/banked statics don't alias.

## Docs to update (on copy to msxmvl)
- `Code-Banking.md`: drop the implicit "no writable statics" assumption; document
  the data model + reserved-region API.
- `tools/README.bankpack.md`: the new `-b _DATA/_BSS` coordination, globals-init,
  SHADOW config, CRLF note.

## Then, downstream
msxzork consumes the enhanced bankpack to finish its ASCII-8 code-banking migration
(hot VM core resident in seg 0, UI/parser in a paged bank, story/image data via the
0xA000 window). Its consumption plan is in the msxzork repo
(`c/docs/ASCII8_CODE_BANKING.md`, `c/docs/BANKPACK_DATA_MODEL_SPEC.md`).
