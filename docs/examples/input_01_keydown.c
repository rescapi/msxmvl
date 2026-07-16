// controls.c — is the jump button held down right now?
//
// The lowest-level keyboard read: Keyboard_Read(row) returns the 8 column bits of one
// matrix row, ACTIVE-LOW (a 0 bit = that key is held this instant). SPACE — our jump
// button — is matrix row 8, bit 0, so it is down when (Keyboard_Read(8) & 1) == 0.
// Reading "is it held" every frame is exactly what you want for jump/thrust/movement.
#include "input.h"

#define JUMP_ROW  8      // SPACE lives in matrix row 8...
#define JUMP_BIT  0x01   // ...as bit 0 (active-low: 0 = pressed)

// True while the player is holding the jump button.
u8 jump_held(void)
{
	return (Keyboard_Read(JUMP_ROW) & JUMP_BIT) == 0;
}

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xE000) R[2];
void main(void)
{
	for (;;) {
		if (jump_held())
			R[0] = 0xA5;                    // SPACE (jump) is currently down
	}
}
