// im2.c — take over the interrupt: your handler IS the 50 Hz tick.
//
// ISR_InstallIM2 routes every interrupt through a RAM vector table straight to
// a plain C function — the whole BIOS interrupt handler (register shuffling,
// keyboard scan, hook chain) is bypassed. This is the fastest way to run
// per-frame game logic, and it works on any program type, because IM 2 never
// needs page 0.
#include "types.h"
#include "isr.h"

volatile u16 g_Ticks;

// An ordinary C function — the module saves/restores registers around it.
void vblank_handler(void)
{
	if (ISR_Status() & 0x80)     // bit 7: this interrupt is a VBLANK
		++g_Ticks;
}

void start_counting(void)
{
	g_Ticks = 0;
	ISR_InstallIM2(vblank_handler);
}

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xE000) R[8];

void main(void)
{
	u16 guard;
	u16 frozen;

	start_counting();

	// the handler now drives g_Ticks all by itself
	for (guard = 60000; guard && g_Ticks < 5; --guard) ;
	R[1] = (g_Ticks >= 5) ? 1 : 0;

	ISR_UninstallIM2();
	frozen = g_Ticks;
	for (guard = 30000; guard; --guard) ;
	R[2] = (g_Ticks == frozen) ? 1 : 0;   // no handler runs after uninstall
	R[3] = (u8)frozen;

	R[0] = (R[1] && R[2]) ? 0xA5 : 0x00;
	for (;;) {}
}
