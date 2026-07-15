// MemSeg_Window(seg) maps a segment AND returns the one that was there, so you can
// restore it — the safe pattern for a temporary switch. Writes go to real mapper
// RAM at 0x8000; different segments are physically distinct memory.
#include "memseg.h"
volatile u8 __at(0xE000) R[4];

void main(void)
{
	MemSeg_Init(16);                        // home = 16
	*(volatile u8*)0x8000 = 0xAA;           // write into segment 16

	MemSeg prev = MemSeg_Window(17);        // map 17, prev == 16
	*(volatile u8*)0x8000 = 0xBB;           // write into segment 17
	MemSeg_Restore(prev);                   // back to 16

	R[1] = prev;                            // 16
	R[2] = *(volatile u8*)0x8000;           // 0xAA — segment 16 kept its value
	R[0] = (prev == 16 && R[2] == 0xAA) ? 0xA5 : 0x00;
	for (;;) {}
}
