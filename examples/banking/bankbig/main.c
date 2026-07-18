// Resident main — acceptance test for BIG-ROM builds (512 KB ASCII-8, 64
// segments). The bank of interest is SEG 63, the LAST segment of the image:
// its far code must be reachable and its statics (initial values, BSS, a
// mutation) must behave — which only works if the file layout really scales
// to SEG * 0x2000 = 0x7E000 and the mapper maps the segment.
#include "types.h"
#include "far.h"

volatile u8 __at(0xE000) R[8];

void main(void)
{
	R[1] = (mid_tag(0) == 0x0C0D) ? 1 : 0;   // a mid-image bank (SEG 5) works
	R[2] = (z_probe(0) == 1) ? 1 : 0;        // SEG 63: statics init + BSS zeroed
	z_mutate(0x31);
	R[3] = (z_readback(0) == 0x62) ? 1 : 0;  // SEG 63: statics are writable RAM
	R[0] = (R[1] & R[2] & R[3]) ? 0xA5 : 0x00;
}
