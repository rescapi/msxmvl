// savecode.c — turn the player's progress into a short "continue" code, and back.
//
// Crypt_Encode maps each byte to exactly two printable characters from a 32-char
// alphabet (size*2 chars, plus a terminator); Crypt_Decode reverses it. A key mixed
// in cyclically, plus a cascade across bytes, means a repeated value never shows up as
// a repeated pair — so the player can't spot the pattern and edit the code by hand.
#include "crypt.h"

#define SAVE_BYTES  4                  // bytes of progress packed into a code

static u8 g_Progress[SAVE_BYTES] = { 0x12, 0x34, 0x56, 0x78 };
static c8 g_Code[SAVE_BYTES * 2 + 1];  // two chars per byte, plus terminator
static u8 g_Restored[SAVE_BYTES];

// Encode the current progress into a continue code the player writes down.
void make_continue_code(void)
{
	Crypt_SetKey("MSX");                    // mandatory before encode/decode
	Crypt_Encode(g_Progress, SAVE_BYTES, g_Code);
}

// Read a continue code back into the saved progress.
void load_continue_code(void)
{
	Crypt_Decode(g_Code, g_Restored);
}

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xE000) R[8];
void main(void)
{
	u8 i, ok = 1;

	make_continue_code();      // progress -> code
	load_continue_code();      // code -> progress again

	for (i = 0; i < SAVE_BYTES; i++)
		if (g_Restored[i] != g_Progress[i])
			ok = 0;

	R[1] = (u8)g_Code[0];      // a printable char from the map
	R[2] = g_Restored[0];      // 0x12 — recovered
	R[3] = g_Restored[3];      // 0x78 — recovered

	R[0] = (ok && g_Code[8] == 0) ? 0xA5 : 0x00;
	for (;;) {}
}
