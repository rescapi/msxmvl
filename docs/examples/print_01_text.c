// hud_text.c — draw HUD text with a bitmap font you supply.
//
// In a bitmap mode (SCREEN 5 / GRAPHIC4) Print_SetBitmapFont selects a font whose glyph
// pixels are drawn straight into the screen bitmap in the current text color. The font is
// just a small header (glyph size, first/last char) followed by 8 bytes per glyph. Set a
// font and color once, then move the cursor and draw characters — your score, lives, and
// on-screen messages.
#include "vdp.h"
#include "print.h"

// A tiny 3-glyph font ('!'..'#'): header {w<<4|h, size, first, last}, then 8 rows/glyph.
static const u8 g_Font[] = {
	0x88, 0x11, 33, 35,
	0x18,0x18,0x18,0x18,0x00,0x18,0x18,0x00,   // '!'  (rows: 0x18 = pixels x3,x4)
	0x66,0x66,0x24,0x00,0x00,0x00,0x00,0x00,   // '"'
	0x24,0x7E,0x24,0x24,0x7E,0x24,0x00,0x00,   // '#'
};

// Set up the HUD font and colors once at startup.
void hud_init(void)
{
	Print_SetBitmapFont(g_Font);
	Print_SetColor(0x0A, 0x01);        // text color 0x0A, background 0x01
}

// Draw one HUD character at column/row (col, row).
void hud_char(u8 col, u8 row, u8 chr)
{
	Print_SetPosition(col, row);
	Print_DrawChar(chr);
}

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xE000) R[8];

void main(void)
{
	VDP_SetMode(VDP_MODE_GRAPHIC4);
	hud_init();
	hud_char(0, 0, '!');
	VDP_CommandWait();                 // let the glyph finish drawing

	// '!' row 0 is 0x18 -> pixels x3,x4 are text-color(A), rest background(1).
	// GRAPHIC4 packs two pixels/byte: byte1 = x2,x3 = 0x1A; byte2 = x4,x5 = 0xA1.
	R[1] = VDP_Peek_16K(0);            // x0,x1 = bg,bg      = 0x11
	R[2] = VDP_Peek_16K(1);            // x2,x3 = bg,text    = 0x1A
	R[3] = VDP_Peek_16K(2);            // x4,x5 = text,bg    = 0xA1
	R[0] = (R[1]==0x11 && R[2]==0x1A && R[3]==0xA1) ? 0xA5 : 0x00;
	for (;;) {}
}
