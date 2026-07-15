// Mem_Set fills a buffer with a byte; Mem_Copy copies one buffer to another.
// The everyday RAM block operations (like memset/memcpy).
#include "memory.h"
volatile u8 __at(0xE000) R[8];

void main(void)
{
	u8 a[4], b[4];

	Mem_Set(0x77, a, 4);           // a = { 77, 77, 77, 77 }
	Mem_Copy(a, b, 4);             // b <- a

	R[1] = a[0]; R[2] = a[3];
	R[3] = b[0]; R[4] = b[3];
	R[0] = (a[0]==0x77 && a[3]==0x77 && b[0]==0x77 && b[3]==0x77) ? 0xA5 : 0x00;
	for (;;) {}
}
