// Example: find the SCC cartridge instead of assuming which slot it is in.
// Build with -DSCC_SLOT_MODE=SCC_SLOT_AUTO.
#include "scc.h"
volatile u8 __at(0xE000) R[8];

static const u8 g_Wave[32] = {
	0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70,
	0x7F, 0x70, 0x60, 0x50, 0x40, 0x30, 0x20, 0x10,
	0x00, 0xF0, 0xE0, 0xD0, 0xC0, 0xB0, 0xA0, 0x90,
	0x81, 0x90, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0,
};

void main(void)
{
	bool found = SCC_Initialize();     // scans the slots, then selects the chip

	R[1] = found ? 1 : 0;              // 0 if no SCC cartridge is present
	R[2] = g_SCC_SlotId;               // the slot it was actually found in

	SCC_LoadWaveform(0, g_Wave);
	R[3] = SCC_GetRegister(SCC_REG_WAVE_1 + 8);   // 0x7F — proves we found it

	R[0] = (found && g_SCC_SlotId != SLOT_NOTFOUND && R[3] == 0x7F) ? 0xA5 : 0x00;
	for (;;) {}
}
