// banks.c — flip which 16 KB bank of mapper RAM is visible in the window.
//
// A RAM mapper holds many 16 KB "banks" (512 KB = 32 of them). Only one shows through the
// window (at 0x8000) at a time; you pick which. MemSeg_SetWindow makes a bank visible;
// MemSeg_GetWindow tells you which one is showing. (The selecting register is write-only, so
// the library keeps a software note — that's what GetWindow reads.)
#include "memseg.h"

#define BANK_TITLE   16     // the bank holding the title screen
#define BANK_LEVEL1  20     // ...and the bank holding level 1

void show_title(void)   { MemSeg_SetWindow(BANK_TITLE); }
void show_level1(void)  { MemSeg_SetWindow(BANK_LEVEL1); }
u8   visible_bank(void) { return MemSeg_GetWindow(); }

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xE000) R[4];
void main(void)
{
	MemSeg_Init(BANK_TITLE);         // start up with the title bank visible
	R[1] = visible_bank();           // -> 16 (BANK_TITLE)
	show_level1();                   // flip to the level-1 bank
	R[2] = visible_bank();           // -> 20 (BANK_LEVEL1)
	R[0] = (R[1] == BANK_TITLE && R[2] == BANK_LEVEL1) ? 0xA5 : 0x00;
	for (;;) {}
}
