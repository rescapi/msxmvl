// SEG 63 — the LAST segment of the 512 KB image. Far code plus the full
// statics data model (an initialized value, BSS, a mutation) living at file
// offset 0x7E000: farrt must map THIS segment to copy the initializers.
#include "types.h"
#include "far.h"

static u16 z_magic = 0x5AA5;
static u8  z_bss[16];
static u16 z_value;

FAR_FN u16 z_probe(u16 unused)
{
	u8 i; u16 ok = 1;
	(void)unused;
	if (z_magic != 0x5AA5) ok = 0;
	for (i = 0; i < 16; ++i) if (z_bss[i] != 0) ok = 0;
	return ok;
}

FAR_FN u16 z_mutate(u16 v)
{
	z_value = v;
	return z_value;
}

FAR_FN u16 z_readback(u16 unused)
{
	(void)unused;
	return z_value + z_value;
}
