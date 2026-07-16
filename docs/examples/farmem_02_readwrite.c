// savegame.c — save and load the hero's name.
//
// For more than a byte or two — a save slot, a sprite, a line of text — copy the whole block
// at once. Far_Write sends a buffer out to mapper RAM; Far_Read fetches it back. The library
// maps the bank once and moves the run with a fast LDIR, instead of paging in and out per byte.
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

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xE000) R[8];
void main(void)
{
	c8 loaded[4];
	MemSeg_Init(16);
	savegame_store_name("ALEX");            // the player named their hero ALEX
	savegame_load_name(loaded);             // ...and later we load it back
	R[1] = loaded[0]; R[2] = loaded[1]; R[3] = loaded[2]; R[4] = loaded[3];
	R[0] = (loaded[0]=='A' && loaded[1]=='L' && loaded[2]=='E' && loaded[3]=='X')
	       ? 0xA5 : 0x00;
	for (;;) {}
}
