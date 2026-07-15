// String_FromUInt16ZT converts a number to a zero-terminated decimal string,
// fixed-width and zero-padded (5 digits for a u16). So 258 becomes "00258".
// (The non-ZT variant String_FromUInt16 writes the same digits without the
// terminating 0, e.g. to append into a larger buffer.)
#include "string.h"
volatile u8 __at(0xE000) R[8];

void main(void)
{
	c8 buf[8];
	String_FromUInt16ZT(258, buf);         // "00258\0"

	R[1] = buf[0]; R[2] = buf[1]; R[3] = buf[2]; R[4] = buf[3]; R[5] = buf[4];
	R[6] = buf[5];                          // 0x00 terminator
	R[0] = (buf[0]=='0' && buf[1]=='0' && buf[2]=='2' && buf[3]=='5' &&
	        buf[4]=='8' && buf[5]=='\0') ? 0xA5 : 0x00;
	for (;;) {}
}
