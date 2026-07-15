// Example: RLEp_UnpackToRAM — unpack a compressed byte stream.
#include "compress.h"
volatile u8 __at(0xE000) R[8];

// RLEp stream: [default] then chunks, terminated by 0x00.
static const u8 g_Packed[] = {
	0xAA,                    // default value for Type-0 runs
	0x02,                    // T0 x3  -> AA AA AA
	0x43, 0xBB,              // T1 x4  -> BB BB BB BB
	0x81, 0xCC, 0xDD,        // T2 x2  -> CC DD CC DD
	0xC2, 0x11, 0x22, 0x33,  // T3 x3  -> 11 22 33 (raw)
	0x00,                    // terminator
};
static u8 g_Out[16];

void main(void)
{
	u16 len = RLEp_UnpackToRAM(g_Packed, g_Out);

	R[1] = (u8)len;      // 14 unpacked bytes
	R[2] = g_Out[0];     // 0xAA  (default run)
	R[3] = g_Out[3];     // 0xBB  (1-byte run)
	R[4] = g_Out[7];     // 0xCC  (2-byte run)
	R[5] = g_Out[11];    // 0x11  (raw copy)

	R[0] = (len == 14 && g_Out[0] == 0xAA && g_Out[3] == 0xBB
	     && g_Out[7] == 0xCC && g_Out[11] == 0x11) ? 0xA5 : 0x00;
	for (;;) {}
}
