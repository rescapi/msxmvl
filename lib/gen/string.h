// msxmvl — clean-room reimplementation of MSXgl "string" module public API.
// Interface (names, signatures, documented behaviour) taken from MSXgl's
// engine/src/string.h header ONLY. No MSXgl implementation source was read.
#ifndef MSXMVL_STRING_H
#define MSXMVL_STRING_H

#include "types.h"

//=============================================================================
// CONFIG (defaults mirror MSXgl config_default.h)
//=============================================================================
#ifndef STRING_USE_FORMAT
#define STRING_USE_FORMAT       TRUE
#endif
#ifndef STRING_USE_INT32
#define STRING_USE_INT32        FALSE
#endif
#ifndef STRING_USE_FROM_UINT8
#define STRING_USE_FROM_UINT8   TRUE
#endif
#ifndef STRING_USE_FROM_UINT16
#define STRING_USE_FROM_UINT16  TRUE
#endif

//=============================================================================
// stdarg.h macros (as declared by MSXgl string.h)
//=============================================================================
typedef u8* va_list;
#define va_start(marker, last)  { marker = (va_list)&last + sizeof(last); }
#define va_arg(marker, type)    *((type *)((marker += sizeof(type)) - sizeof(type)))
#define va_copy(dest, src)      { dest = src; }
#define va_end(marker)          { marker = (va_list) 0; };

//=============================================================================
// Group: Character (inline, defined in header exactly like MSXgl)
//=============================================================================

// Check if a given character is numeric (0-9)
inline bool Char_IsNum(c8 chr)
{
	return (chr >= '0') && (chr <= '9');
}

// Check if a given character is alphabetic (A-Za-z)
inline bool Char_IsAlpha(c8 chr)
{
	return ((chr >= 'A') && (chr <= 'Z')) || ((chr >= 'a') && (chr <= 'z'));
}

// Check if a given character is alphanumeric
inline bool Char_IsAlphaNum(c8 chr)
{
	return Char_IsNum(chr) || Char_IsAlpha(chr);
}

//=============================================================================
// Group: String
//=============================================================================

// Get the length of a given string
inline u8 String_Length(const c8* str)
{
	u8 ret = 0;
	while (*str++)
		ret++;
	return ret;
}

// Copy a source string to a destination buffer (zero-terminated)
inline void String_Copy(c8* dst, const c8* src)
{
	while (*src)
		*dst++ = *src++;
	*dst = '\0';
}

#if (STRING_USE_FROM_UINT8)
// Create a zero-terminated string from a 8-bit unsigned integer.
// Buffer must be >= 4 bytes; result is 3 digits padded with '0' + '\0'.
void String_FromUInt8ZT(u8 value, c8* string);

// Create a (non zero-terminated) string from a 8-bit unsigned integer.
// Buffer must be >= 3 bytes; result is 3 digits padded with '0'.
void String_FromUInt8(u8 value, c8* string);
#endif

#if (STRING_USE_FROM_UINT16)
// Create a zero-terminated string from a 16-bit unsigned integer.
// Buffer must be >= 6 bytes; result is 5 digits padded with '0' + '\0'.
void String_FromUInt16ZT(u16 value, c8* string);

// Create a (non zero-terminated) string from a 16-bit unsigned integer.
// Buffer must be >= 5 bytes; result is 5 digits padded with '0'.
void String_FromUInt16(u16 value, c8* string);
#endif

#if (STRING_USE_FORMAT)
// Build a zero-terminated string. Format: %[0]<padding><type>
//   Types: i/d 16-bit signed, I/D 32-bit signed (STRING_USE_INT32),
//          x 16-bit hex, X 32-bit hex (STRING_USE_INT32),
//          c 8-bit char, s zero-terminated string.
//   Padding: leading width digits; a leading 0 pads with '0' else with space.
void String_Format(c8* dest, const c8* format, ...);
#endif

#endif // MSXMVL_STRING_H
