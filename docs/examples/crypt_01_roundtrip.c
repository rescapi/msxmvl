// Example: Crypt_Encode / Crypt_Decode — obfuscate a save-game blob.
#include "crypt.h"
volatile u8 __at(0xE000) R[8];

static u8 g_Score[4] = { 0x12, 0x34, 0x56, 0x78 };
static c8 g_Cipher[4 * 2 + 1];   // size*2 + 1
static u8 g_Back[4];

void main(void)
{
	u8 i, ok = 1;

	Crypt_SetKey("MSX");                    // mandatory before encode/decode
	Crypt_Encode(g_Score, 4, g_Cipher);     // 4 bytes -> 8 chars + terminator
	Crypt_Decode(g_Cipher, g_Back);         // and back again

	for (i = 0; i < 4; i++)
		if (g_Back[i] != g_Score[i])
			ok = 0;

	R[1] = (u8)g_Cipher[0];    // a printable char from the map
	R[2] = g_Back[0];          // 0x12 — recovered
	R[3] = g_Back[3];          // 0x78 — recovered

	R[0] = (ok && g_Cipher[8] == 0) ? 0xA5 : 0x00;
	for (;;) {}
}
