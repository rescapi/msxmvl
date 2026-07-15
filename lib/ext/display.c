// msxmvl extension: SCREEN 5/6 double-buffer page management. See display.h.
// SDCC --sdcccall 1. Self-contained (drives VDP port 0x99 directly).
#include "display.h"

u8 g_DispFront;   // page shown on screen
u8 g_DispBack;    // page being drawn (hidden)

// plain u8 -> void : arg `page` in A. Writes R#2 = (page<<5) | 0x1F, which sets the
// GRAPHIC4 name-table base to page*0x8000 (the VDP then scans out that page).
void Display_ShowPage(u8 page) __naked
{
	page;
	__asm
		rlca
		rlca
		rlca
		rlca
		rlca                 ; A = page << 5  (page 0..3 -> 0x00/0x20/0x40/0x60)
		or   a, #0x1F        ; low bits of R#2 are always 1 in GRAPHIC4
		out  (0x99), a       ; VDP data = value
		ld   a, #(0x80 | 2)  ; select R#2
		out  (0x99), a
		ret
	__endasm;
}

void Display_InitDoubleBuffer(void)
{
	g_DispFront = 0;
	g_DispBack = 1;
	Display_ShowPage(0);
}

void Display_Flip(void)
{
	u8 t = g_DispFront;
	g_DispFront = g_DispBack;   // reveal the page we just drew
	g_DispBack = t;             // draw into the page we were showing
	Display_ShowPage(g_DispFront);
}

u8 Display_GetFrontPage(void) { return g_DispFront; }
u8 Display_GetBackPage(void)  { return g_DispBack; }
u16 Display_GetBackPageY(void) { return (u16)g_DispBack << 8; }
