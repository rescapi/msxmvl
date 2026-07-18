// msxmvl EXTENSION module: isr -- IM 2 interrupt takeover (see isr.h).
#include "types.h"
#include "isr.h"

// the live handler + the status byte captured at interrupt entry
static volatile ISR_Handler g_ISR_Handler;
static volatile u8 g_ISR_S0;

u8 ISR_Status(void) { return g_ISR_S0; }

// The dispatcher every IM 2 interrupt lands on (via the RAM thunk). Naked:
// this IS the interrupt frame, so it does its own save/restore and reti.
static void isr_im2_dispatch(void) __naked
{
	__asm
	push af
	push bc
	push de
	push hl
	push ix
	push iy
	in a, (0x99)            ; read VDP S#0: acknowledges the interrupt
	ld (_g_ISR_S0), a
	ld hl, (_g_ISR_Handler)
	ld de, #00001$
	push de
	jp (hl)                 ; call the C handler (plain sdcccall function)
00001$:
	pop iy
	pop ix
	pop hl
	pop de
	pop bc
	pop af
	ei
	reti
	__endasm;
}

void ISR_InstallIM2(ISR_Handler handler)
{
	u8* p = (u8*)(((u16)ISR_IM2_TABLE_PAGE) << 8);
	u8* thunk = (u8*)((((u16)ISR_IM2_THUNK_PAGE) << 8) | ISR_IM2_THUNK_PAGE);
	u16 i;

	g_ISR_Handler = handler;

	// 257 bytes: any bus byte the Z80 fetches as the vector low half still
	// lands on thunk_page:thunk_page -- the classic all-same-byte IM 2 table.
	for (i = 0; i < 257; ++i) p[i] = ISR_IM2_THUNK_PAGE;
	thunk[0] = 0xC3;                          // jp isr_im2_dispatch
	*(void**)(thunk + 1) = (void*)isr_im2_dispatch;

	__asm
	di
	ld a, #ISR_IM2_TABLE_PAGE
	ld i, a
	im 2
	ei
	__endasm;
}

void ISR_UninstallIM2(void)
{
	__asm
	di
	im 1
	ei
	__endasm;
}
