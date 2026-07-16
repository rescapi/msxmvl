// SEGMENT WINDOWING — the raw mechanism behind "banking".
//
// The Z80 can only address 64 KB at once, but an MSX memory mapper holds much more
// (512 KB and up), split into 16 KB "segments" (think: interchangeable cartridges).
// You choose which segment is visible through a fixed "window" at address 0x8000.
// Only ONE segment shows through the window at a time; swapping is instant.
//
// Snag: the hardware register that selects the segment is WRITE-ONLY — you can set it
// but not read it back. So the library keeps a software note of the current segment.
// MemSeg_Init picks a starting ("home") segment; MemSeg_SetWindow swaps another one in;
// MemSeg_GetWindow tells you which is showing right now.
//
// Here: a game keeps its title screen in one segment and level 1 in another, and flips
// the window between them.
#include "memseg.h"
volatile u8 __at(0xE000) R[4];

#define SEG_TITLE   16      // 16 KB segment holding the title screen
#define SEG_LEVEL1  20      // ...and the one holding level 1

void main(void)
{
	MemSeg_Init(SEG_TITLE);              // start with the title-screen segment in the window
	R[1] = MemSeg_GetWindow();           // which segment is showing? -> 16 (SEG_TITLE)

	MemSeg_SetWindow(SEG_LEVEL1);        // flip to the level-1 segment
	R[2] = MemSeg_GetWindow();           // now showing -> 20 (SEG_LEVEL1)

	R[0] = (R[1] == SEG_TITLE && R[2] == SEG_LEVEL1) ? 0xA5 : 0x00;
	for (;;) {}
}
