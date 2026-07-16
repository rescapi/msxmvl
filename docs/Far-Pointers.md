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

Mapper RAM is divided into **16 KB segments** (512 KB = 32 of them). A **far pointer** is just a
label for "byte *N* inside segment *S*", written `FAR(segment, offset)`. Think of it like a house
address: the segment is the street, the offset is the number on the door. You hand that address to
a `Far_*` function and it does the rest — swaps the right segment into view, reads or writes, and
swaps your own memory back — so from your side it feels like ordinary memory that just happens to
be huge.

```c
typedef u32 FarPtr;                 // one 32-bit value: segment * 16KB + offset
#define FAR(seg, ofs)  ...          // make a handle: segment + offset (0..0x3FFF)
#define FAR_ADD(p, n)  ...          // advance a handle; it carries across segments for you
```

---

## Read and write one byte — `Far_Peek` / `Far_Poke`

The simplest access: one byte at a time. Great for a score, a flag, a counter.

```c
// FAR POINTERS — reach one byte of data that lives beyond the Z80's 64 KB.
//
// A "far pointer" is a handle to a byte somewhere in the big mapper RAM, written
// FAR(segment, offset). Far_Poke writes one byte through it; Far_Peek reads one back.
// You never touch the mapper hardware or the 0x8000 window yourself — the library maps
// the right segment in, does the access, and maps your old one back.
//
// Here: two players' high scores, kept in two different segments. Because the segments
// are separate physical memory, player 1's score can never clobber player 2's — no
// bookkeeping, no overlap.
#include "farmem.h"
volatile u8 __at(0xE000) R[4];

#define SEG_PLAYER1  17     // segment holding player 1's save data
#define SEG_PLAYER2  18     // ...and player 2's
#define OFF_HISCORE  0      // where in the segment the high score lives

void main(void)
{
	MemSeg_Init(16);                                    // bring up the mapper (home = 16)

	Far_Poke(FAR(SEG_PLAYER1, OFF_HISCORE), 95);        // player 1 scored 95
	Far_Poke(FAR(SEG_PLAYER2, OFF_HISCORE), 72);        // player 2 scored 72

	R[1] = Far_Peek(FAR(SEG_PLAYER1, OFF_HISCORE));     // read them back -> 95
	R[2] = Far_Peek(FAR(SEG_PLAYER2, OFF_HISCORE));     // -> 72

	R[0] = (R[1] == 95 && R[2] == 72) ? 0xA5 : 0x00;    // both intact, side by side
	for (;;) {}
}
```

Runs to `R[] = a5 5f 48` (`0x5F = 95`, `0x48 = 72`). Two segments, two scores, zero chance of one
overwriting the other. *(tested: `farmem_01_pokepeek.c`)*

> Per-byte access re-maps the segment on every call. For more than a handful of bytes, use the
> block copy below — it maps once and moves the whole run at full speed.

---

## Copy a whole block — `Far_Read` / `Far_Write`

For anything bigger than a byte — a save slot, a sprite, a line of text — copy the whole block at
once. `Far_Write` sends a block out to mapper RAM; `Far_Read` fetches one back.

```c
// FAR POINTERS — copy a whole block to/from mapper RAM in one go.
//
// Poking one byte at a time is fine for a score, but for a chunk of data — a save
// slot, a sprite pattern, a line of text — use Far_Write to send a block out and
// Far_Read to fetch it back. The library maps the segment once and copies the whole
// run with a fast LDIR, instead of paging in and out for every byte.
//
// Here: save the hero's name into a segment, then load it back — exactly what a
// "continue?" screen does when it reads your save.
#include "farmem.h"
volatile u8 __at(0xE000) R[8];

#define SEG_SAVE   19       // segment we use as a save slot
#define OFF_NAME   0        // offset of the name field within the slot

void main(void)
{
	c8 hero_name[4] = { 'A', 'L', 'E', 'X' };   // the name the player entered
	c8 loaded[4];                               // where we will read it back into
	MemSeg_Init(16);

	Far_Write(FAR(SEG_SAVE, OFF_NAME), hero_name, 4);   // SAVE: store the 4 letters
	Far_Read(FAR(SEG_SAVE, OFF_NAME), loaded, 4);       // LOAD: read them back later

	R[1] = loaded[0]; R[2] = loaded[1]; R[3] = loaded[2]; R[4] = loaded[3];   // 'A' 'L' 'E' 'X'
	R[0] = (loaded[0]=='A' && loaded[1]=='L' && loaded[2]=='E' && loaded[3]=='X')
	       ? 0xA5 : 0x00;
	for (;;) {}
}
```

Runs to `R[] = a5 41 4c 45 58` — the ASCII for `A L E X`. That's a save-and-load round trip.
*(tested: `farmem_02_readwrite.c`)*

### Data bigger than one segment just works

Segments are 16 KB. A big world map or a long song can be larger than that, so a block might
**start in one segment and finish in the next**. You don't split it yourself — `Far_Read`,
`Far_Write` and `Far_Set` notice the edge and carry on across it.

```c
// FAR POINTERS — data bigger than one segment "just works".
//
// Segments are 16 KB. A big world map, a long song, a wide tile row — these can be
// larger than a single segment, so a block you read or write may START in one segment
// and CONTINUE in the next. You do NOT have to split it yourself: Far_Write, Far_Read
// and Far_Set notice the 16 KB edge and carry on into the following segment for you.
//
// Here: a strip of map tiles is stored right at the end of the level segment, so it
// spills over into the next one. We write it as a single block and read it back whole —
// the crossing is invisible. FAR(seg, 0x3FFE) is 2 bytes before the end of a segment.
#include "farmem.h"
volatile u8 __at(0xE000) R[8];

#define SEG_MAP    19       // first segment of a level map that is bigger than 16 KB

void main(void)
{
	u8 tiles[4] = { 10, 11, 12, 13 };   // four map tiles to store
	u8 loaded[4];                       // read them back here
	MemSeg_Init(16);

	// Start 2 bytes before the segment's end, so tiles 12 and 13 land in the NEXT segment.
	Far_Write(FAR(SEG_MAP, 0x3FFE), tiles, 4);      // one call — the split is automatic
	Far_Read (FAR(SEG_MAP, 0x3FFE), loaded, 4);     // read the whole strip back

	R[1] = Far_Peek(FAR(SEG_MAP,     0x3FFF));       // tile 11 — last byte of segment 19
	R[2] = Far_Peek(FAR(SEG_MAP + 1, 0x0000));       // tile 12 — first byte of segment 20
	R[0] = (loaded[0]==10 && loaded[1]==11 && loaded[2]==12 && loaded[3]==13 &&
	        R[1]==11 && R[2]==12) ? 0xA5 : 0x00;
	for (;;) {}
}
```

Runs to `R[] = a5 0b 0c` — tile `11` was the last byte of segment 19, tile `12` the first byte of
segment 20, and the block read back whole. *(tested: `farmem_03_boundary.c`)*

---

## Fill a region — `Far_Set`

Clearing a map to an "empty" tile, or zeroing a buffer before reuse — one call, and it crosses
segment boundaries like the block functions.

```c
// FAR POINTERS — fill a far region with one value.
//
// Before you draw a new level you usually clear its map to a single "empty" tile;
// before you reuse a buffer you zero it. Far_Set does that out in mapper RAM: it writes
// one repeated byte across a run, and — like Far_Write — it crosses the 16 KB segment
// boundary for you, so clearing a big buffer that spans segments is still one call.
//
// Here: clear a stretch of level map to the "grass" tile (id 7). The stretch sits at the
// end of one segment and runs into the next, and Far_Set fills both halves.
#include "farmem.h"
volatile u8 __at(0xE000) R[4];

#define SEG_MAP    22       // level-map segment (the run continues into segment 23)
#define TILE_GRASS 7        // the tile id we clear the map to

void main(void)
{
	MemSeg_Init(16);

	Far_Set(FAR(SEG_MAP, 0x3FFE), TILE_GRASS, 4);   // clear 4 tiles, spanning seg 22 -> 23

	R[1] = Far_Peek(FAR(SEG_MAP,     0x3FFF));       // grass, in segment 22
	R[2] = Far_Peek(FAR(SEG_MAP + 1, 0x0000));       // grass, in segment 23 (past the edge)
	R[0] = (R[1] == TILE_GRASS && R[2] == TILE_GRASS) ? 0xA5 : 0x00;
	for (;;) {}
}
```

Runs to `R[] = a5 07 07` — grass on both sides of the segment boundary. *(tested: `farmem_05_set.c`)*

---

## Work in place, safely — `Far_With`

Copying data out is one option; sometimes you'd rather reach *into* far data and read a field
directly. The danger with that is a plain pointer into a segment: the moment anything swaps the
window, that pointer is aimed at the wrong memory. `Far_With` removes the danger — it maps the
segment, gives your callback a normal, valid pointer, and restores the previous segment when the
callback returns. The pointer lives only inside the callback, so it can never dangle.

```c
// FAR POINTERS — reach into far data in place, safely.
//
// Copying data out (Far_Read) is one option; sometimes you'd rather reach into it where
// it lives and read a field directly. But a plain pointer into a segment is risky: the
// instant anything else swaps the window, that pointer is aimed at the wrong memory.
//
// Far_With makes that safe. You hand it a far pointer and a small callback; it maps the
// segment in, gives your callback an ordinary, valid pointer to the data, and — when the
// callback returns — restores whatever segment was there before. The pointer exists ONLY
// inside the callback, so it can never be left dangling.
//
// Here: a monster's stats live in far RAM. We reach in and read its HP field directly,
// without copying the whole struct out first.
#include "farmem.h"
volatile u8 __at(0xE000) R[4];

#define SEG_ENEMIES  21     // segment holding the enemy stat tables

typedef struct { u8 hp; u8 attack; } Monster;

static u8 g_hp;                                     // the callback leaves what it read here
static void read_hp(const void* mapped, void* ctx)
{
	(void)ctx;
	const Monster* m = (const Monster*)mapped;      // a normal pointer — valid right here
	g_hp = m->hp;                                   // read the HP field in place
}

void main(void)
{
	MemSeg_Init(16);
	Far_Poke(FAR(SEG_ENEMIES, 0), 100);             // give this monster 100 HP (the hp field)

	Far_With(FAR(SEG_ENEMIES, 0), sizeof(Monster), read_hp, (void*)0);   // read it in place

	R[1] = g_hp;                                    // 100 — pulled straight from far RAM
	R[2] = MemSeg_GetWindow();                      // 16 — Far_With put our segment back
	R[0] = (g_hp == 100 && R[2] == 16) ? 0xA5 : 0x00;
	for (;;) {}
}
```

Runs to `R[] = a5 64 10` — the callback read `100` HP (`0x64`) straight from far RAM, and
afterwards the window is back on the home segment (`0x10 = 16`). *(tested: `farmem_04_with.c`)*

> **Golden rule:** never keep a pointer into the window after `Far_With` returns (or after a
> `MemSeg_Window`). Read the bytes out with `Far_Read`, or do the work inside the callback.
> `Far_With` exists to make that the easy, safe path.

## See also

- [Segment Windowing](Segment-Windowing.md) — the `memseg` layer underneath, if you want to map a
  segment and run your own code against it.
- [Code Banking](Code-Banking.md) — the sibling trick for *code*: running more program than fits
  in 64 KB (and it works on MSX1 too).
