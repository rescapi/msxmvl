# Far Pointers (`farmem`)

**This is what lets an MSX game be big.** The Z80 can only "see" 64 KB of memory at any instant —
but an MSX2 with a memory mapper physically holds **512 KB, 1 MB, or more**. Far pointers are how
you use all of it: store more levels, sprites, text, maps, and music than could ever fit in 64 KB,
and reach any byte of it with a simple handle — no juggling of hardware registers, no manual
paging.

If you've hit "I'm out of RAM" on an MSX2, this is the page that fixes it.

> Link `memseg.c farmem.c`, include `farmem.h`. Call `MemSeg_Init(home)` **once** at startup
> before any `Far_*` call. Needs an MSX2 RAM mapper (see [the MSX1 note](Segment-Windowing.md)).

## The idea in one picture

Mapper RAM is divided into **16 KB banks** (512 KB = 32 of them). A **far pointer** is just a label
for "byte *N* inside bank *B*", written `FAR(bank, offset)`. Think of it like a house address: the
bank is the street, the offset is the number on the door. You hand that address to a `Far_*`
function and it does the rest — swaps the right bank into view, reads or writes, and swaps your own
memory back — so from your side it feels like ordinary memory that just happens to be huge.

```c
typedef u32 FarPtr;                 // one 32-bit value: bank * 16KB + offset
#define FAR(bank, ofs)  ...         // make a handle: bank + offset (0..0x3FFF)
```

---

## Read and write one byte — `Far_Peek` / `Far_Poke`

The simplest access: one byte at a time. Great for a score, a flag, a counter. Give each player
their own bank and their high scores can never overwrite each other:

```c
// hiscore.c — a per-player high-score table that lives out in mapper RAM.
#include "farmem.h"

#define BANK_P1  17     // player 1's save data has its own 16 KB bank
#define BANK_P2  18     // ...and player 2's

// Store a player's high score.
void hiscore_save(u8 player_bank, u8 score)
{
	Far_Poke(FAR(player_bank, 0), score);
}

// Read a player's high score back.
u8 hiscore_load(u8 player_bank)
{
	return Far_Peek(FAR(player_bank, 0));
}
```

`hiscore_save(BANK_P1, 95)` and `hiscore_save(BANK_P2, 72)` store into two separate banks;
`hiscore_load` reads each back. Two scores, side by side, zero chance of collision. *(tested:
`farmem_01_pokepeek.c`)*

> Per-byte access re-maps the bank on every call. For more than a handful of bytes, use the block
> copy below — it maps once and moves the whole run at full speed.

---

## Copy a whole block — `Far_Read` / `Far_Write`

For anything bigger than a byte — a save slot, a sprite, a line of text — copy the whole block at
once. `Far_Write` sends a block out to mapper RAM; `Far_Read` fetches one back.

```c
// savegame.c — save and load the hero's name.
#include "farmem.h"

#define BANK_SAVE  19       // the bank we use as a save slot
#define OFF_NAME   0        // where the name field sits inside the slot

// Write the hero's name into the save slot.
void savegame_store_name(const c8* name)
{
	Far_Write(FAR(BANK_SAVE, OFF_NAME), name, 4);
}

// Read the hero's name back (e.g. for the "continue?" screen).
void savegame_load_name(c8* out)
{
	Far_Read(FAR(BANK_SAVE, OFF_NAME), out, 4);
}
```

Store `"ALEX"`, load it back later — a save-and-load round trip in two calls. *(tested:
`farmem_02_readwrite.c`)*

### Data bigger than one bank just works

A world map or a long song can be larger than a 16 KB bank, so a run of bytes might **start in one
bank and finish in the next**. You don't split it yourself — `Far_Read`, `Far_Write` and `Far_Set`
notice the edge and carry on across it. Treat the whole map as one long array:

```c
// worldmap.c — a world map too big for 64 KB, stored across mapper banks.
#include "farmem.h"

#define BANK_MAP  19    // first bank of a world map that spans several banks

// Read a run of tiles from the map, starting at any tile index — even one that
// makes the run cross from one bank into the next. It "just works".
void map_read(u16 tile_index, u8* out, u16 count)
{
	Far_Read(FAR(BANK_MAP, tile_index), out, count);
}

// Write a run of tiles back.
void map_write(u16 tile_index, const u8* tiles, u16 count)
{
	Far_Write(FAR(BANK_MAP, tile_index), tiles, count);
}
```

Write a 4-tile strip starting 2 tiles before a bank's end and it spills into the next bank; read it
back and you get the whole strip, the crossing invisible. *(tested: `farmem_03_boundary.c`)*

---

## Fill a region — `Far_Set`

Clearing a level's map to an "empty" tile, or zeroing a buffer before reuse — one call, and it
crosses bank boundaries like the block functions.

```c
// level.c — clear a level's map to a single tile before drawing it.
#include "farmem.h"

#define BANK_MAP    22      // the level-map bank (a big map continues into bank 23)
#define TILE_GRASS  7       // the tile id we clear the map to

// Fill a run of the map with one tile — used to reset the map between levels.
void map_fill(u16 tile_index, u8 tile, u16 count)
{
	Far_Set(FAR(BANK_MAP, tile_index), tile, count);
}
```

`map_fill(start, TILE_GRASS, count)` paints grass across the run — across a bank boundary if the
run reaches one. *(tested: `farmem_05_set.c`)*

---

## Work in place, safely — `Far_With`

Copying data out is one option; sometimes you'd rather reach *into* far data and read a field
directly. The danger is a plain pointer into a bank: the moment anything swaps the window, that
pointer is aimed at the wrong memory. `Far_With` removes the danger — it maps the bank, gives your
callback a normal, valid pointer, and restores the previous bank when the callback returns. The
pointer lives only inside the callback, so it can never dangle.

```c
// enemy.c — read a monster's stats where they live, without copying them out.
#include "farmem.h"

typedef struct { u8 hp; u8 attack; } Monster;

static u8 s_hp;
static void grab_hp(const void* mapped, void* ctx)
{
	(void)ctx;
	s_hp = ((const Monster*)mapped)->hp;    // a normal pointer — valid right here
}

// Read a monster's HP straight out of its bank (no copy of the whole struct).
u8 enemy_hp(u8 bank)
{
	Far_With(FAR(bank, 0), sizeof(Monster), grab_hp, (void*)0);
	return s_hp;
}
```

`enemy_hp(bank)` reads the `hp` field straight out of far RAM and returns it — and afterwards the
window is back exactly where it was. *(tested: `farmem_04_with.c`)*

> **Golden rule:** never keep a pointer into the window after `Far_With` returns (or after a
> `MemSeg_Window`). Read the bytes out with `Far_Read`, or do the work inside the callback.
> `Far_With` exists to make that the easy, safe path.

## See also

- [Segment Windowing](Segment-Windowing.md) — the `memseg` layer underneath, if you want to map a
  bank and run your own code against it.
- [Code Banking](Code-Banking.md) — the sibling trick for *code*: running more program than fits in
  64 KB (and it works on MSX1 too).
