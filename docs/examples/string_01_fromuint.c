// hud.c — render the player's score to text for the on-screen display.
//
// String_FromUInt16ZT converts a number to a zero-terminated decimal string, fixed-width
// and zero-padded (5 digits for a u16), so 258 becomes "00258". Fixed width keeps the
// score field a constant size on screen, no jitter as the score grows. (The non-ZT
// String_FromUInt16 writes the same digits without the terminating 0, for appending into
// a larger HUD string.)
#include "string.h"

// Format a score into a caller-supplied buffer (room for 5 digits + terminator).
void score_to_text(u16 score, c8* out)
{
	String_FromUInt16ZT(score, out);
}

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xE000) R[8];
void main(void)
{
	c8 buf[8];
	score_to_text(258, buf);               // "00258\0"

	R[1] = buf[0]; R[2] = buf[1]; R[3] = buf[2]; R[4] = buf[3]; R[5] = buf[4];
	R[6] = buf[5];                          // 0x00 terminator
	R[0] = (buf[0]=='0' && buf[1]=='0' && buf[2]=='2' && buf[3]=='5' &&
	        buf[4]=='8' && buf[5]=='\0') ? 0xA5 : 0x00;
	for (;;) {}
}
