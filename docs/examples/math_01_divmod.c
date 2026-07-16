// hud.c — turn a score into decimal digits for the on-screen display.
//
// The Z80 has no divide instruction, and a general division routine is big and slow.
// Math_Div10 and Math_Mod10 are the fast special case you actually need to render a
// number: peel off one decimal digit at a time (Mod10 = this digit, Div10 = the rest).
#include "math.h"

// The ones digit of a score value (0..9) — the next character to draw.
u8 score_ones(u8 score)
{
	return Math_Mod10(score);
}

// Drop the ones digit, leaving the tens-and-up part still to be drawn.
u8 score_shift_down(u8 score)
{
	return (u8)Math_Div10(score);
}

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xE000) R[4];
void main(void)
{
	R[1] = score_shift_down(95);   // 95 / 10 = 9
	R[2] = score_ones(95);         // 95 % 10 = 5
	R[0] = (R[1] == 9 && R[2] == 5) ? 0xA5 : 0x00;
	for (;;) {}
}
