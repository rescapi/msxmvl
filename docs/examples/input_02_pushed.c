// controls.c — did the player just press fire this frame (a fresh press, not a hold)?
//
// Edge detection: Keyboard_IsKeyPushed(KEY_SPACE) is true only on the frame SPACE goes
// from up to down — one shot per press, ideal for firing a shot or confirming a menu. It
// compares this scan against the previous one, so call Keyboard_Update() once per frame
// first. Point the scan at your OWN buffer with Keyboard_SetBuffer, or the module shares
// the BIOS keyboard work area that the interrupt handler also writes (phantom presses).
#include "input.h"

static u8 kb_new[11], kb_old[11];      // this game's key state (current + previous)

// Call once at startup so the game owns its key-state buffers.
void controls_init(void)
{
	Keyboard_SetBuffer(kb_new, kb_old);
}

// Call once per frame; true only on the frame FIRE (SPACE) was pressed.
u8 fire_pressed(void)
{
	Keyboard_Update();                 // rescan: old <- new, new <- matrix
	return Keyboard_IsKeyPushed(KEY_SPACE);
}

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xE000) R[2];
void main(void)
{
	controls_init();

	for (;;) {
		if (fire_pressed())
			R[0] = 0xA5;                    // fired on the SPACE press edge
	}
}
