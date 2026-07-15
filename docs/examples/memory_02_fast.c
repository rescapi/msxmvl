// Mem_FastCopy / Mem_FastSet are unrolled-LDI variants: faster for larger blocks,
// same result. Use them on hot copy/clear paths where the size is worth it.
#include "memory.h"
volatile u8 __at(0xE000) R[8];

void main(void)
{
	u8 a[8], b[8];

	Mem_FastSet(0x5A, a, 8);       // a = eight 0x5A bytes
	Mem_FastCopy(a, b, 8);         // b <- a

	R[1] = b[0]; R[2] = b[7];
	R[0] = (b[0]==0x5A && b[7]==0x5A) ? 0xA5 : 0x00;
	for (;;) {}
}
