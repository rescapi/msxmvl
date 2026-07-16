# String Conversion (`string`)

Turn numbers into text for display. Link `string.c`, include `string.h`. (`c8` is the character
type in `types.h`.)

---

## `String_FromUInt16ZT` / `String_FromUInt16`

```c
void String_FromUInt16ZT(u16 value, c8* string);   // zero-terminated
void String_FromUInt16  (u16 value, c8* string);    // no terminator
```

Convert a `u16` to a **fixed-width, zero-padded** decimal string — 5 digits for a `u16`, so `258`
becomes `"00258"`. The `ZT` variant appends a `\0`; the plain variant writes just the digits (to
append into a larger buffer). 8-bit variants (`String_FromUInt8ZT` / `String_FromUInt8`) produce
3 digits.

Drawing the score to the HUD, you format it into a small buffer and then blit that text. The
fixed 5-digit width keeps the field from jittering as the score grows:

```c
// hud.c — render the player's score to text for the on-screen display.
#include "string.h"

// Format a score into a caller-supplied buffer (room for 5 digits + terminator).
void score_to_text(u16 score, c8* out)
{
	String_FromUInt16ZT(score, out);
}
```

`score_to_text(258, buf)` fills `buf` with `"00258"` and a trailing `\0` — the ASCII
`30 30 32 35 38 00`, ready to draw. *(tested: `string_01_fromuint.c`)*

> **Fixed width is a feature** for aligned displays (scores, timers) — the leading zeros keep the
> field a constant size. If you want no leading zeros, skip them when drawing, or use
> `Print_DrawInt` from the [print](VDP-Access.md) module which formats minimally.

## See also

- [Math Utilities](Math-Utilities.md) — `Math_Div10` / `Math_Mod10`, the digit extraction behind
  number formatting.
