// Print renders text through a font YOU supply. In a bitmap mode (SCREEN 5 /
// GRAPHIC4) Print_SetBitmapFont selects a font whose glyph pixels are drawn into
// the screen bitmap in the text color. This tiny font (4-byte header + three 8x8
// glyphs for '!'..'#') is enough to draw and verify one character.
#include "vdp.h"
#include "print.h"
volatile u8 __at(0xE000) R[8];

// header: {w<<4|h nibbles, size, firstChar, lastChar}, then 8 bytes per glyph
static const u8 g_Font[] = {
	0x88, 0x11, 33, 35,
	0x18,0x18,0x18,0x18,0x00,0x18,0x18,0x00,   // '!'  (rows: 0x18 = pixels x3,x4)
	0x66,0x66,0x24,0x00,0x00,0x00,0x00,0x00,   // '"'
	0x24,0x7E,0x24,0x24,0x7E,0x24,0x00,0x00,   // '#'
};

void main(void)
{
	VDP_SetMode(VDP_MODE_GRAPHIC4);
	Print_SetBitmapFont(g_Font);
	Print_SetColor(0x0A, 0x01);        // text color 0x0A, background 0x01
	Print_SetPosition(0, 0);
	Print_DrawChar('!');
	VDP_CommandWait();                 // let the glyph finish drawing

	// '!' row 0 is 0x18 -> pixels x3,x4 are text-color(A), rest background(1).
	// GRAPHIC4 packs two pixels/byte: byte1 = x2,x3 = 0x1A; byte2 = x4,x5 = 0xA1.
	R[1] = VDP_Peek_16K(0);            // x0,x1 = bg,bg      = 0x11
	R[2] = VDP_Peek_16K(1);            // x2,x3 = bg,text    = 0x1A
	R[3] = VDP_Peek_16K(2);            // x4,x5 = text,bg    = 0xA1
	R[0] = (R[1]==0x11 && R[2]==0x1A && R[3]==0xA1) ? 0xA5 : 0x00;
	for (;;) {}
}
