// VDP_RegWrite(reg, value) writes any VDP control register directly — the escape
// hatch for anything the typed helpers don't cover. Here we set R#7 (border /
// backdrop color) to color 10; the harness reads R#7 back from hardware.
#include "vdp.h"
volatile u8 __at(0xE000) R[2];

void main(void)
{
	VDP_RegWrite(7, 0x0A);         // R#7 = backdrop color 10
	R[0] = 0xA5;                   // reached here; R#7 value checked by the harness
	for (;;) {}
}
