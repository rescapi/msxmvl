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

```c
#include "string.h"
volatile u8 __at(0xE000) R[8];

void main(void)
{
	c8 buf[8];
	String_FromUInt16ZT(258, buf);         // "00258\0"

	R[1] = buf[0]; R[2] = buf[1]; R[3] = buf[2]; R[4] = buf[3]; R[5] = buf[4];
	R[6] = buf[5];                          // 0x00 terminator
	R[0] = (buf[0]=='0' && buf[1]=='0' && buf[2]=='2' && buf[3]=='5' &&
	        buf[4]=='8' && buf[5]=='\0') ? 0xA5 : 0x00;
	for (;;) {}
}
```

Runs to `R[] = a5 30 30 32 35 38 00` — the ASCII for `"00258"` followed by the terminator.
*(tested: `string_01_fromuint.c`)*

> **Fixed width is a feature** for aligned displays (scores, timers) — the leading zeros keep the
> field a constant size. If you want no leading zeros, skip them when drawing, or use
> `Print_DrawInt` from the [print](VDP-Access.md) module which formats minimally.

## See also

- [Math Utilities](Math-Utilities.md) — `Math_Div10` / `Math_Mod10`, the digit extraction behind
  number formatting.
