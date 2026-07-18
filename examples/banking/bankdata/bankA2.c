// Bank 5, SECOND .rel — the dedup regression case. Both bankA.rel and this file
// live in seg 5 and both call the resident helper res_helper(). bankpack must
// inject `-g _res_helper=<addr>` only ONCE for the bank; without the dedup guard
// sdld aborts on a duplicate definition and the ROM never links.
#include "types.h"

u16 res_helper(u16 x);         // resident; shared with bankA

static u16 a2_magic = 0x2222;  // this .rel carries its own initialized static too
static u8  a2_bss[4];

u16 a2_check(u16 unused)
{
	u8 i; u16 ok = 1;
	(void)unused;
	if (a2_magic != 0x2222) ok = 0;
	for (i = 0; i < 4; ++i) if (a2_bss[i] != 0) ok = 0;
	if (res_helper(0x10) != (u16)(0x10 + 0x1234)) ok = 0;   // shared resident symbol
	return ok;
}
