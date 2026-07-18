// Bank (16 KB, ASCII-16) — carries a const table BIGGER THAN 8 KB. Under an
// 8 KB mapper the table's tail would be unmapped garbage; the checksum in
// main.c proves the full 16 KB segment is really there. Plus the usual
// data-model statics.
#include "types.h"
#include "bank_big.h"   // generated: BIG_LEN + big_table[]

static u16 a_magic = 0xA55A;
static u8  a_bss[16];

u16 a_probe(u16 unused)
{
	u8 i; u16 ok = 1;
	(void)unused;
	if (a_magic != 0xA55A) ok = 0;
	for (i = 0; i < 16; ++i) if (a_bss[i] != 0) ok = 0;
	return ok;
}

u16 a_sum(u16 unused)
{
	u16 i, sum = 0;
	(void)unused;
	for (i = 0; i < BIG_LEN; ++i) sum += big_table[i];
	return sum;
}
