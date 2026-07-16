// verify.c — accept a typed-in continue code only if it decodes cleanly.
//
// A continue code the player writes down (or edits) is trusted only after
// Crypt_Decode accepts it: the function returns FALSE the moment it meets a character
// that isn't in the alphabet — which is exactly how you catch a typo, or a tampered
// save code, instead of loading garbage.
#include "crypt.h"

// Verify + decode a continue code. FALSE means "reject it, show WRONG CODE".
bool verify_code(const c8* code, u8* progress)
{
	return Crypt_Decode(code, progress);
}

// ---- test harness (not shown in the docs) --------------------------------
//
// The round-trip example proves the happy path. This harness proves the edges,
// because crypt is hand-written assembly and a 4-byte round-trip does not exercise:
//   the early-exit when a byte XORs to ZERO, key cycling, the failure paths, or a
//   caller-supplied map/code table.
volatile u8 __at(0xE000) R[8];

// Deliberately includes 0x00 and 0xFF, and a byte that XORs to zero against the key.
static const u8 g_Data[8] = { 0x00, 0xFF, 0x41, 0x00, 0x7F, 0x80, 0x01, 0xFE };
static u8 g_Back[8];
static c8 g_Str[20];

// A caller-supplied map and code table (the API promises these work).
static const c8  g_MyMap[33]  = "0123456789abcdefghijklmnopqrstuv";
static const u16 g_MyCode[8]  = { 0x0100, 0x0200, 0x0400, 0x0800,
                                  0x0001, 0x0002, 0x0004, 0x0008 };

void main(void)
{
	u8 i, ok = 1;

	// --- 1. no key set -> both calls must FAIL, not scribble ---------------
	Crypt_SetKey((const c8*)0);
	if (Crypt_Encode(g_Data, 8, g_Str))  ok = 0;
	if (verify_code("AAAA", g_Back))     ok = 0;
	R[1] = ok;

	// --- 2. round trip, 1-char key (wraps on EVERY byte), zeros in data ----
	Crypt_SetKey("Z");
	if (!Crypt_Encode(g_Data, 8, g_Str)) ok = 0;
	for (i = 0; i < 8; ++i) g_Back[i] = 0xAA;
	if (!verify_code(g_Str, g_Back))     ok = 0;
	for (i = 0; i < 8; ++i)
		if (g_Back[i] != g_Data[i]) ok = 0;
	R[2] = (u8)(g_Str[16] == 0);         // terminated at size*2

	// --- 3. every output character must come from the map ------------------
	for (i = 0; i < 16; ++i)
	{
		u8 j = 0;
		while ((j < 32) && (g_CryptMap[j] != g_Str[i])) ++j;
		if (j == 32) ok = 0;             // emitted a character outside the map
	}

	// --- 4. a character that is not in the map -> Decode must FAIL ---------
	g_Str[3] = '!';
	if (verify_code(g_Str, g_Back))      ok = 0;

	// --- 5. caller-supplied map AND code table -> still round-trips --------
	Crypt_SetMap(g_MyMap);
	Crypt_SetCode(g_MyCode);
	Crypt_SetKey("key");                 // 3-char key over 8 bytes: wraps twice
	if (!Crypt_Encode(g_Data, 8, g_Str)) ok = 0;
	for (i = 0; i < 8; ++i) g_Back[i] = 0xAA;
	if (!verify_code(g_Str, g_Back))     ok = 0;
	for (i = 0; i < 8; ++i)
		if (g_Back[i] != g_Data[i]) ok = 0;
	R[3] = (u8)g_Str[0];                 // a character from the CUSTOM map

	R[0] = (ok && R[1] && R[2]) ? 0xA5 : 0x00;
	for (;;) {}
}
