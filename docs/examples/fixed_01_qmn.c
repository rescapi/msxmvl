// Fixed-point numbers are named Qm.n: m integer bits, n fraction bits. QMN_Set8
// packs an integer into a Q format, QMN_Get8 unpacks it. In Q7.1 (7 integer, 1
// fraction bit) the value is stored x2, so 10 -> 20 and back.
#include "fixed_point.h"
volatile u8 __at(0xE000) R[4];

void main(void)
{
	i8 packed = QMN_Set8(Q7_1, 10);        // 10 in Q7.1 -> 20
	i8 back    = QMN_Get8(Q7_1, packed);   // 20 -> 10

	R[1] = (u8)packed;                     // 20
	R[2] = (u8)back;                       // 10
	R[0] = (packed == 20 && back == 10) ? 0xA5 : 0x00;
	for (;;) {}
}
