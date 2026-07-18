# Game Saves (`sram`, `pac`)

Battery-backed **cartridge saves** — for the game on a diskless MSX. Real MSX hardware has two
answers, and this library covers both with one block-level API. Link `sram.c`, include `sram.h`.

| Backend | Hardware | Where the SRAM lives | Size |
|---|---|---|---|
| `SRAM_BACKEND_PAC` (default) | **PAC / FM-PAC** cartridge (Panasoft SW-M001/SW-M004) | a *separate* cartridge, any slot | 8190 bytes |
| `SRAM_BACKEND_MAPPER` | **mapper-SRAM MegaROM** (Hydlide 2, the Koei titles, A-Train…) | a chip on *your own* bankpack cartridge | 2/8/32 KB |

The two protocols share no register, address or discovery mechanism, so the honest common ground
is the **block level** — and that is the whole public surface:

```c
bool Sram_Detect(void);              // is a battery-backed SRAM actually there?
u16  Sram_Size(void);                // usable bytes (PAC: 8190)
void Sram_Read (u16 ofs, void* dst, u16 n);
void Sram_Write(u16 ofs, const void* src, u16 n);
bool Sram_Verify(u16 ofs, const void* src, u16 n);   // read-back compare
```

Every call opens, copies and **closes** the SRAM internally — the PAC is relocked and the mapper
window restored before the call returns, so a crash mid-game can never leave saves writable.
A span that does not fit `Sram_Size()` is rejected whole: no partial saves. `Sram_Verify` is the
battery check — run it after every write, because a dead battery fails there and nowhere else.

## Saving to a PAC cartridge

The PAC's 8 KB SRAM sits at `0x4000-0x5FFD` in *its* slot, behind a lock: write `'M'` to `0x5FFE`
and `'i'` to `0x5FFF` to unlock, anything else to relock. `Sram_Detect` probes every (sub)slot
with that protocol — RAM fails it (writable while locked), ROM fails it (never writable) — and
remembers the first PAC in `g_Sram_Slot`. Access is per-byte interslot (BIOS `RDSLT`/`WRSLT`),
so it works from any program: ROM cartridge, disk, anywhere.

```c
// savegame_pac.c — save the game to a PAC cartridge: battery-backed, no disk.
#include "sram.h"

typedef struct { u8 level; u8 lives; u16 score; u8 name[4]; } SaveGame;

// TRUE if a PAC (or FM-PAC) is present in any slot. Call once at startup.
bool save_init(void)
{
	return Sram_Detect();              // remembers the slot in g_Sram_Slot
}

// Store the save block at the start of the SRAM and prove it stuck.
bool save_store(const SaveGame* s)
{
	Sram_Write(0, s, sizeof(SaveGame));
	return Sram_Verify(0, s, sizeof(SaveGame));   // read-back = battery health
}

// Load it back (a fresh PAC reads as 0xFF bytes — check your own record).
void save_fetch(SaveGame* s)
{
	Sram_Read(0, s, sizeof(SaveGame));
}
```

After `save_store(...)`, `save_fetch(...)` returns the same bytes — and a raw interslot read of
`0x4000` still sees `0xFF`, proving the SRAM was relocked on the way out.
*(tested: `sram_01_pac.c`, on the openMSX `pac` extension — the SRAM-only SW-M001, no
copyrighted ROM involved; passes on C-BIOS MSX1 and MSX2)*

## Sharing the PAC like MSXgl does (`pac`)

A PAC is **shared between games by design** — commercial titles carve it into 8 pages of 1 KB and
mark ownership with a 4-byte signature. The `pac` module is the MSXgl-compatible surface for that
convention (same names, same semantics — a thin layer over the same core; link `pac.c sram.c` and
build with `-DAPPSIGN='"MVZ8"'`, your own four characters):

```c
// pacpages_msxgl.c — the MSXgl-style PAC surface: 8 pages of 1 KB, signed saves.
#include "pac.h"

// Find every PAC in the machine and select the first. FALSE = no PAC anywhere.
bool pac_boot(void)
{
	return PAC_Initialize();
}

// Claim page 2 for this game: erase it, then write a signed record.
void pac_save(const u8* data, u16 size)
{
	PAC_Format(2);                     // page 2 -> all 0xFF (erases any signature)
	PAC_Write(2, data, size);          // writes APPSIGN, then the data after it
}

// Whose data is in page 2 right now? PAC_CHECK_APP once pac_save has run.
u8 pac_state(void)
{
	return PAC_Check(2);
}

// Read the record back (the signature bytes are skipped, mirroring PAC_Write).
void pac_load(u8* data, u16 size)
{
	PAC_Read(2, data, size);
}
```

`PAC_Check` before you write, `PAC_CHECK_APP`/`PAC_CHECK_EMPTY`/`PAC_CHECK_UNDEF` after —
**never format a page that checks as `PAC_CHECK_UNDEF`**: that is another game's save.
`PAC_Initialize` records up to `PAC_DEVICE_MAX` cartridges in `g_PAC_Slot[]`; `PAC_Select`
switches between them. Page 7 holds only 1022 bytes — the lock registers live at its end.
*(tested: `pac_01_compat.c`)*

## Saving to your own cartridge (mapper-SRAM)

Since [bankpack](Code-Banking.md) builds ASCII8/ASCII16 MegaROMs, your cartridge format can grow
a save chip — the way Hydlide 2 or the Koei titles did it. The SRAM is an *extra segment* behind
the normal bank-select register: one line in the manifest declares it, and the build validates it
(sizes per mapper; a select value colliding with a code segment is a hard error):

```
MAPPER ASCII8
SRAM 8            # 8 KB chip -> boot with openMSX -romtype ASCII8SRAM8
```

`bankpack --gen` emits `SRAM_SELECT`/`SRAM_SIZE`/`SRAM_SHADOW`/`SRAM_BANKREG` into `far.h`;
compile `lib/ext/sram.c` with `-DSRAM_BACKEND_MAPPER` and link it into the **resident** bank.
The same five `Sram_` calls now hit the chip through the `0x8000` window: each call disables
interrupts, saves the current segment from the shadow byte (the registers are write-only),
selects the SRAM, copies with one `ldir`, restores, and re-enables — a fast local copy instead
of per-byte interslot calls, and no open/close to forget. `Sram_Detect` RAM-tests one byte
through the window, so a build accidentally run on a cartridge without the chip reports FALSE
instead of "saving" into ROM.

*(tested: `test/banksram/run.sh` — ASCII8SRAM8 on C-BIOS MSX1+MSX2, ASCII16SRAM2, ASCII8SRAM32
with a span straddling an 8 KB block boundary, far calls intact around every save, and
**persistence across a second boot** via openMSX's SRAM file; plus the manifest negatives)*

## Which one do you want?

- **PAC**: zero hardware cost to you, works with every game that speaks the protocol — but the
  player must own one, and 8 KB is shared between all their games. Sign your pages.
- **Mapper-SRAM**: always present (it is *your* cartridge), all yours, fast — but it exists only
  if the cartridge is actually manufactured with the chip. Emulators and flash carts support all
  the wirings (`ASCII8SRAM2/8/32`, `ASCII16SRAM2/8`).

Nothing stops you from shipping both: detect the mapper chip first, fall back to a PAC scan.

## See also

- [Real-Time Clock](Real-Time-Clock.md) — 6 bytes of CMOS: enough for a high score, no cartridge needed.
- [Encryption](Encryption.md) — password saves, when there is no battery anywhere.
- [Code Banking](Code-Banking.md) — the MegaROM builder the mapper backend plugs into.
- `docs/SRAM.md` — the hardware ground truth (lock protocol, select values, openMSX romtypes).
