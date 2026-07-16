// SEGMENT WINDOWING — borrow a segment, then put things back.
//
// Most of the time your game runs from its "home" segment. Now and then you need to
// reach into another one — read a tile from the level-data segment, copy a sprite out
// of an asset segment — and then carry on as before.
//
// MemSeg_Window(seg) swaps a segment into the window AND hands back the one that was
// there, so you can MemSeg_Restore() it afterwards. This is the safe pattern: you can
// never "forget" which segment you were on. Whatever you write through the window at
// 0x8000 goes into REAL mapper RAM — each segment is its own physical memory, so the
// home segment keeps its contents while you are away.
#include "memseg.h"
volatile u8 __at(0xE000) R[4];

#define SEG_HOME    16      // the segment the game normally runs from
#define SEG_ASSETS  17      // a segment we want to peek into for a moment

void main(void)
{
	MemSeg_Init(SEG_HOME);                  // home segment is in the window
	*(volatile u8*)0x8000 = 0xAA;           // store something in home (a health value, say)

	MemSeg prev = MemSeg_Window(SEG_ASSETS); // borrow the assets segment; remember home (prev)
	*(volatile u8*)0x8000 = 0xBB;           // this write lands in SEG_ASSETS, not home
	MemSeg_Restore(prev);                   // done — put the home segment back

	R[1] = prev;                            // prev is 16 (SEG_HOME): we knew where to return
	R[2] = *(volatile u8*)0x8000;           // 0xAA — home never lost its value while we were away
	R[0] = (prev == SEG_HOME && R[2] == 0xAA) ? 0xA5 : 0x00;
	for (;;) {}
}
