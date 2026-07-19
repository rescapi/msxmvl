// pause_and_progress.c -- nested screens via a state stack, and progress flags.
//
// StateStack lets "pause" sit on top of "play" and pop back exactly where you were;
// Flags packs booleans 8-per-byte (256 switches in 32 bytes -- cheap in an SRAM save).
// Here both are exercised deterministically so the behaviour is self-verifying.
#include "game_state.h"

enum { ST_PLAY = 10, ST_PAUSE = 20, ST_MENU = 30 };

// ---- test harness --------------------------------------------------------
volatile u8 __at(0xE000) R[8];
void main(void)
{
	StateStack st;
	u8 flags[4];               // 32 flags
	u8 ok = 1, i;

	// --- state stack: push MENU/PLAY/PAUSE, then pop back down ---
	StateStack_Init(&st);
	StateStack_Push(&st, ST_MENU);
	StateStack_Push(&st, ST_PLAY);
	StateStack_Push(&st, ST_PAUSE);
	R[1] = StateStack_Count(&st);          // 3
	R[2] = StateStack_Top(&st);            // 20 (PAUSE)
	R[3] = StateStack_Pop(&st);            // 20, back to PLAY
	R[4] = StateStack_Top(&st);            // 10 (PLAY)
	StateStack_Pop(&st);                   // -> MENU
	StateStack_Pop(&st);                   // -> empty
	R[5] = StateStack_Pop(&st);            // STATE_NONE (0xFF) when empty
	if (R[1] != 3 || R[2] != ST_PAUSE || R[3] != ST_PAUSE ||
	    R[4] != ST_PLAY || R[5] != STATE_NONE)
		ok = 0;

	// full: 8 pushes succeed, the 9th fails
	StateStack_Init(&st);
	for (i = 0; i < STATE_STACK_MAX; i++)
		if (!StateStack_Push(&st, i)) ok = 0;
	if (StateStack_Push(&st, 99)) ok = 0;  // stack full -> must fail
	R[6] = StateStack_Count(&st);          // 8

	// --- packed flags: set across byte boundaries, toggle, reset ---
	Flags_Clear(flags, 4);
	Flags_Set(flags, 0);
	Flags_Set(flags, 7);
	Flags_Set(flags, 8);                    // second byte
	Flags_Set(flags, 31);                   // last flag
	if (!Flags_Get(flags, 0) || !Flags_Get(flags, 7) ||
	    !Flags_Get(flags, 8) || !Flags_Get(flags, 31)) ok = 0;
	if (Flags_Get(flags, 1) || Flags_Get(flags, 9)) ok = 0;   // untouched
	Flags_Toggle(flags, 0);                 // 1 -> 0
	Flags_Toggle(flags, 1);                 // 0 -> 1
	Flags_Reset(flags, 7);                  // 1 -> 0
	if (Flags_Get(flags, 0) || !Flags_Get(flags, 1) || Flags_Get(flags, 7)) ok = 0;
	R[7] = flags[0];                        // bit1 set only -> 0x02

	R[0] = ok ? 0xA5 : 0x00;
	for (;;) {}
}
