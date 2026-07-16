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
