# Text Output (`print`)

Render text — strings, numbers, formatted output — into any screen mode, with optional shadow and
outline effects. Link `vdp.c vdp_inl.c print.c`, include `print.h`.

## How print works

`print` keeps a **font**, a **cursor position**, and text/background **colors** as state, then draws
glyphs at the cursor and advances it. The font is provided by *you* (a small header describing glyph
size + the glyph bitmaps), so the same API serves bitmap modes (SCREEN 5–8), text modes (SCREEN 0/1),
VRAM fonts, and even sprite-rendered text. You pick the font *kind* with the matching setter:

| Setter | Renders text as |
|--------|-----------------|
| `Print_SetBitmapFont` | pixels drawn into a GRAPHIC4–7 bitmap (command engine) |
| `Print_SetTextFont` | tile indices in a text/GRAPHIC1 name table |
| `Print_SetVRAMFont` | a font pre-loaded in VRAM |
| `Print_SetSpriteFont` | hardware sprites |

## Setup and drawing (tested)

Provide a font, choose the render path with the matching setter, set colors and cursor, then draw.
This minimal font holds three 8×8 glyphs (`'!'`..`'#'`); drawing `'!'` and reading the bitmap back
proves the glyph pixels landed in the text color.

```c
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
```

Runs to `R[] = a5 11 1a a1` — the `'!'` glyph's top row landed exactly where the font defines it.
*(tested: `print_01_text.c`)*

Everyday drawing is higher-level — `Print_DrawText("SCORE ")`, `Print_DrawInt(1234)`,
`Print_DrawChar('!')` — with the cursor advancing after each.

## Drawing functions

```c
void Print_DrawChar(u8 chr);              void Print_DrawText(const c8* string);
void Print_DrawInt(i16 value);            void Print_DrawHex8(u8 value);
void Print_DrawHex16(u16 value);          void Print_DrawBin8(u8 value);
void Print_DrawFormat(const c8* fmt, ...); // printf-style: %i %x %c %s ...
void Print_Clear(void);                    void Print_Backspace(u8 num);
```

`Print_DrawInt` formats a number minimally (no leading zeros — contrast
[`String_FromUInt16`](String-Conversion.md), which is fixed-width). `Print_SetPosition` moves the
cursor; drawing advances it.

## Effects

```c
void Print_SetShadow(bool enable, i8 offsetX, i8 offsetY, u8 color);
void Print_SetOutline(bool enable, u8 color);
```

Enable a drop shadow or a 1-pixel outline; subsequent text picks it up. Both are two-pass (draw the
effect color offset/around, then the text on top) and are compiled in only if the corresponding
`PRINT_USE_FX_*` option is on.

## See also

- [String Conversion](String-Conversion.md) — build number strings when you want fixed width.
- [VDP Access](VDP-Access.md) / [Bitmap Drawing](Bitmap-Drawing.md) — the layers `print` draws through.
