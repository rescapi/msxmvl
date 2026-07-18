// page0.c — a 48 KB cartridge: pages 0, 1 AND 2 are ROM; the BIOS is gone.
//
// crt0_rom48 (EX_ROM=48) switches page 0 to the cartridge after boot, which
// buys 16 KB more ROM (0x0000-0x3FFF, great for const data) and gives the
// program the interrupt vector itself: 0x0038 jumps through a RAM pointer,
// with a default handler that just acknowledges the VDP. The price is the
// contract: after boot there are NO BIOS calls, ever — the library talks to
// the hardware directly anyway.
#include "types.h"

// Const data living in PAGE 0 — impossible while the BIOS occupies it.
__at(0x0100) const u8 g_Page0Table[64] = {
	  0,   5,  10,  15,  20,  25,  30,  35,  40,  45,  50,  55,  60,  65,  70,  75,
	 80,  85,  90,  95, 100, 105, 110, 115, 120, 125, 130, 135, 140, 145, 150, 155,
	160, 165, 170, 175, 180, 185, 190, 195, 200, 205, 210, 215, 220, 225, 230, 235,
	240, 245, 250, 255,   4,   9,  14,  19,  24,  29,  34,  39,  44,  49,  54,  59
};

u16 page0_checksum(void)
{
	u8 i; u16 sum = 0;
	for (i = 0; i < 64; ++i) sum += g_Page0Table[i];
	return sum;
}

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xE000) R[8];

void main(void)
{
	volatile u16 spin;
	u16 sum;

	// page 0 really is OUR ROM now: 0x0000 holds our jp stub (0xC3), where the
	// BIOS ROM has di (0xF3); 0x0038 holds our vector jp.
	R[1] = (*(volatile u8*)0x0000 == 0xC3) ? 1 : 0;
	R[2] = (*(volatile u8*)0x0038 == 0xC3) ? 1 : 0;

	sum = page0_checksum();
	R[3] = (u8)sum;
	R[4] = (u8)(sum >> 8);

	// interrupts are live under our own vector: survive a few frames of them
	for (spin = 0; spin < 30000; ++spin) ;
	R[5] = 1;

	R[0] = (R[1] && R[2] && sum == 7008 && R[5]) ? 0xA5 : 0x00;
	for (;;) {}
}
