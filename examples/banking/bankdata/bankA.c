// Bank 5 — banked module with real writable statics.
#include "types.h"

static u16 a_magic = 0xA55A;   // initialized: farrt must copy this bank's DATA slice
static u8  a_bss[16];          // BSS: farrt must zero it
static u16 a_value = 0x1111;   // initialized + mutated later (aliasing check)

u16 a_probe(u16 unused)
{
	u8 i; u16 ok = 1;
	(void)unused;
	if (a_magic != 0xA55A) ok = 0;
	if (a_value != 0x1111) ok = 0;
	for (i = 0; i < 16; ++i) if (a_bss[i] != 0) ok = 0;
	return ok;
}

u16 a_mutate(u16 v)
{
	a_value = v;
	a_bss[0] = (u8)v;
	return a_value;
}

u16 a_readback(u16 unused)
{
	(void)unused;
	return a_value + a_bss[0];
}
