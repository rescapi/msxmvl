// Edge detection: Keyboard_IsKeyPushed(KEY_x) is true only on the frame a key goes
// from up to down (a fresh press, not a hold). It compares this scan against the
// previous one, so call Keyboard_Update() once per frame first.
//
// Point the scan at your OWN buffer with Keyboard_SetBuffer — otherwise the module
// shares the BIOS keyboard work area, which the interrupt handler also writes.
#include "input.h"
volatile u8 __at(0xE000) R[2];

static u8 kb_new[11], kb_old[11];      // this program's key state (current + previous)

void main(void)
{
	Keyboard_SetBuffer(kb_new, kb_old);

	for (;;) {
		Keyboard_Update();                 // rescan: old <- new, new <- matrix
		if (Keyboard_IsKeyPushed(KEY_SPACE))
			R[0] = 0xA5;                    // fired on the SPACE press edge
	}
}
