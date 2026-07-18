// Second 16 KB bank — statics only (the aliasing side of the data model).
#include "types.h"

static u16 b_magic = 0x5AA5;
static u8  b_bss[16];
static u16 b_value = 0x2222;

u16 b_probe(u16 unused)
{
	u8 i; u16 ok = 1;
	(void)unused;
	if (b_magic != 0x5AA5) ok = 0;
	if (b_value != 0x2222) ok = 0;
	for (i = 0; i < 16; ++i) if (b_bss[i] != 0) ok = 0;
	return ok;
}

u16 b_mutate(u16 v)
{
	b_value = v;
	b_bss[0] = (u8)v;
	return b_value;
}

u16 b_readback(u16 unused)
{
	(void)unused;
	return b_value + b_bss[0];
}
