// msxmvl — clean-room reimplementation of MSXgl "string" module (bodies).
// Signatures + documented behaviour from MSXgl engine/src/string.h only.
// No MSXgl implementation source was read.
#include "string.h"

//=============================================================================
// 8-bit unsigned -> decimal
//=============================================================================
#if (STRING_USE_FROM_UINT8)

// 3 fixed digits, zero padded, no terminator.
// 3 fixed digits, zero padded, no terminator. Tight 8-bit subtraction (sub sets borrow when the
// place value exceeds the remainder). sdcccall(1): value->A, string->DE.
void String_FromUInt8(u8 value, c8* string) __naked
{
	(void)value; (void)string;
	__asm
		ld   b, #0x30 - 1    ; hundreds
	1$:
		inc  b
		sub  #100
		jr   nc, 1$
		add  a, #100         ; restore overshoot
		push af              ; save remainder
		ld   a, b
		ld   (de), a
		inc  de
		pop  af
		ld   b, #0x30 - 1    ; tens
	2$:
		inc  b
		sub  #10
		jr   nc, 2$
		add  a, #10
		push af
		ld   a, b
		ld   (de), a
		inc  de
		pop  af
		add  a, #0x30        ; units
		ld   (de), a
		ret
	__endasm;
}

// Same, then zero-terminated.
void String_FromUInt8ZT(u8 value, c8* string)
{
	String_FromUInt8(value, string);
	string[3] = '\0';
}

#endif // STRING_USE_FROM_UINT8

//=============================================================================
// 16-bit unsigned -> decimal
//=============================================================================
#if (STRING_USE_FROM_UINT16)

// 5 fixed digits, zero padded, no terminator.
// sdcccall(1): value(u16)->HL, string(c8*)->DE. Tight power-of-10 subtraction via `add hl,bc`
// (bc = -power): carry set <=> HL >= power, so we count subtractions then restore the overshoot
// with a single `sbc hl,bc`. ~11 cycles/iteration + ~41 bytes — beats the C division/subtract loops.
void String_FromUInt16(u16 value, c8* string) __naked
{
	(void)value; (void)string;
	__asm
		ld   bc, #-10000
		call 1$
		ld   bc, #-1000
		call 1$
		ld   bc, #-100
		call 1$
		ld   bc, #-10
		call 1$
		ld   a, l            ; units digit (HL now 0..9)
		add  a, #0x30
		ld   (de), a
		ret
	1$:
		ld   a, #0x30 - 1    ; '0' - 1
	2$:
		inc  a
		add  hl, bc          ; HL -= power; carry set while HL was >= power
		jr   c, 2$
		sbc  hl, bc          ; overshot (carry=0): restore HL += power
		ld   (de), a
		inc  de
		ret
	__endasm;
}

// Same, then zero-terminated.
void String_FromUInt16ZT(u16 value, c8* string)
{
	String_FromUInt16(value, string);
	string[5] = '\0';
}

#endif // STRING_USE_FROM_UINT16

//=============================================================================
// String_Format
//=============================================================================
#if (STRING_USE_FORMAT)

// Emit an unsigned decimal value (no padding, no sign) into buf built from the
// end; returns pointer to first produced digit. len receives digit count.
static c8* Num_Dec16(u16 value, c8* end, u8* len)
{
	u8 n = 0;
	c8* p = end;
	*p = '\0';
	do
	{
		u16 q = value / 10;
		u8 r = (u8)(value - q * 10);
		*--p = (c8)('0' + r);
		value = q;
		n++;
	} while (value);
	*len = n;
	return p;
}

static const c8 g_HexDigit[16] =
	{ '0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F' };

#if (STRING_USE_INT32)
static c8* Num_Dec32(u32 value, c8* end, u8* len)
{
	u8 n = 0;
	c8* p = end;
	*p = '\0';
	do
	{
		u32 q = value / 10;
		u8 r = (u8)(value - q * 10);
		*--p = (c8)('0' + r);
		value = q;
		n++;
	} while (value);
	*len = n;
	return p;
}
#endif // STRING_USE_INT32

void String_Format(c8* dest, const c8* format, ...)
{
	va_list args;
	c8 buf[12];
	c8 c;

	va_start(args, format);

	while ((c = *format++) != '\0')
	{
		if (c != '%')
		{
			*dest++ = c;
			continue;
		}

		c = *format++;
		if (c == '%')			// literal percent
		{
			*dest++ = '%';
			continue;
		}
		if (c == '\0')			// dangling '%'
		{
			*dest++ = '%';
			break;
		}

		// Parse [0]<width>
		bool zero = FALSE;
		u8 width = 0;
		if (c == '0')
		{
			zero = TRUE;
			c = *format++;
		}
		while ((c >= '0') && (c <= '9'))
		{
			width = (u8)(width * 10 + (c - '0'));
			c = *format++;
		}

		// Non-padded specifiers first
		if (c == 's')
		{
			const c8* s = va_arg(args, const c8*);
			while (*s)
				*dest++ = *s++;
			continue;
		}
		if (c == 'c')
		{
			// char is promoted to int in a variadic call
			u16 v = va_arg(args, u16);
			*dest++ = (c8)v;
			continue;
		}

		// Hex specifiers: emit a fixed-width nibble dump of the value's low
		// nibbles, most-significant first, always zero-filled. An explicit
		// width sets the digit count (capped at the value's nibble count);
		// width 0 defaults to the full width of the type. The '0' flag and
		// space padding do not apply to hex.
		if (c == 'x')
		{
			u16 v = va_arg(args, u16);
			u8 hw = width ? width : 4;
			if (hw > 4) hw = 4;
			while (hw) { hw--; *dest++ = g_HexDigit[(u8)((v >> (hw * 4)) & 0x0F)]; }
			continue;
		}
#if (STRING_USE_INT32)
		if (c == 'X')
		{
			u32 v = va_arg(args, u32);
			u8 hw = width ? width : 8;
			if (hw > 8) hw = 8;
			while (hw) { hw--; *dest++ = g_HexDigit[(u8)((v >> (hw * 4)) & 0x0F)]; }
			continue;
		}
#endif // STRING_USE_INT32

		// %u / %U: MSXgl's String_Format leaves these as no-op stubs -- they emit
		// nothing and consume no argument. Match that behaviour exactly (emitting the
		// literal letter, as the default case would, and/or consuming the arg would both
		// diverge from MSXgl). Note Print_DrawFormat *does* implement %u.
		if (c == 'u')
			continue;
#if (STRING_USE_INT32)
		if (c == 'U')
			continue;
#endif

		// Decimal specifiers -> produce a digit body, then emit sign followed
		// by padding to <width> (padding counts digits only, not the sign).
		c8* body;
		u8 len = 0;
		bool neg = FALSE;

		switch (c)
		{
		case 'i':
		case 'd':
			{
				i16 v = va_arg(args, i16);
				u16 u;
				if (v < 0) { neg = TRUE; u = (u16)(-v); }
				else u = (u16)v;
				body = Num_Dec16(u, &buf[11], &len);
			}
			break;

#if (STRING_USE_INT32)
		case 'I':
		case 'D':
			{
				i32 v = va_arg(args, i32);
				u32 u;
				if (v < 0) { neg = TRUE; u = (u32)(-v); }
				else u = (u32)v;
				body = Num_Dec32(u, &buf[11], &len);
			}
			break;
#endif // STRING_USE_INT32

		default:
			// Unknown specifier: emit it verbatim.
			*dest++ = c;
			continue;
		}

		// sign first, then padding (space or zero), then digits
		if (neg)
			*dest++ = '-';
		while (width > len) { *dest++ = zero ? '0' : ' '; width--; }
		while (len--) *dest++ = *body++;
	}

	*dest = '\0';
	va_end(args);
}

#endif // STRING_USE_FORMAT
