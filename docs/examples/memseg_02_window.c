// banks.c — borrow another bank for a moment, then restore yours (the safe pattern).
//
// Most of the time your game runs from its "home" bank. Now and then you reach into another —
// to read a tile, copy a sprite — and then carry on. MemSeg_Window swaps a bank in AND hands
// back the one that was there, so you can always return to exactly where you were. Each bank is
// its own physical RAM, so home keeps its contents while you are away.
#include "memseg.h"

#define BANK_HOME    16     // the bank the game normally runs from
#define BANK_ASSETS  17     // a bank we want to write into for a moment

// Write one byte into the assets bank, then come straight back to the home bank.
void poke_asset(u8 value)
{
	MemSeg prev = MemSeg_Window(BANK_ASSETS);   // borrow the assets bank; remember the old one
	*(volatile u8*)0x8000 = value;              // this write lands in the assets bank
	MemSeg_Restore(prev);                       // put the old bank back
}

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xE000) R[4];
void main(void)
{
	MemSeg_Init(BANK_HOME);
	*(volatile u8*)0x8000 = 0xAA;    // store 0xAA in the home bank
	poke_asset(0xBB);                // write into the assets bank, then restore home
	R[1] = MemSeg_GetWindow();       // 16 — we're back on the home bank
	R[2] = *(volatile u8*)0x8000;    // 0xAA — home was untouched while we were away
	R[0] = (R[1] == BANK_HOME && R[2] == 0xAA) ? 0xA5 : 0x00;
	for (;;) {}
}
