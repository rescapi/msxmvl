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

## A HUD text layer (tested)

A game HUD sets up its font and colors once, then just moves the cursor and stamps characters —
the score, the lives counter, a message. Here a minimal font of three 8×8 glyphs (`'!'`..`'#'`)
stands in for a real HUD font; drawing `'!'` and reading the bitmap back proves the glyph pixels
landed in the text color.

```c
// hud_text.c — draw HUD text with a bitmap font you supply.
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
```

Drawing `'!'` at the top-left corner writes its top row (`0x18`, pixels x3/x4) into the bitmap:
GRAPHIC4 packs two pixels per byte, so those pixels read back as `0x1A` and `0xA1` (text color
`A` against background `1`) exactly where the font defines them. *(tested: `print_01_text.c`)*

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
