// Display_ShowPage(p) is the low-level "make the VDP scan out page p" — it writes
// VDP R#2 = (p<<5) | 0x1F. Here we select page 3; R#2 must read 0x7F on hardware.
// (Init/Flip call this internally; use it directly only for manual page control.)
#include "display.h"
volatile u8 __at(0xE000) R[2];

void main(void)
{
	Display_ShowPage(3);           // R#2 = (3<<5)|0x1F = 0x7F
	R[0] = 0xA5;                   // reached here without hanging; R#2 checked by harness
	for (;;) {}
}
