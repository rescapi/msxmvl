// Lowest-level keyboard read: Keyboard_Read(row) returns the 8 column bits of one
// matrix row, ACTIVE-LOW (a 0 bit = that key is held down right now). SPACE is
// matrix row 8, bit 0, so it is down when (Keyboard_Read(8) & 1) == 0.
// (The test harness holds SPACE down via the emulator.)
#include "input.h"
volatile u8 __at(0xE000) R[2];

void main(void)
{
	for (;;) {
		u8 row8 = Keyboard_Read(8);         // bit 0 = SPACE (0 = pressed)
		if ((row8 & 1) == 0)
			R[0] = 0xA5;                    // SPACE is currently down
	}
}
