// msxmvl clean-room reimplementation of MSXgl "basic_usr" module.
// Public names + signatures match MSXgl exactly (drop-in replacement).
// Derived from MSXgl engine/src/basic_usr.h (interface only) + MSX BIOS knowledge.
//
// The module is a thin accessor over the BASIC USR() interchange BIOS variables:
//   M_VALTYP (0xF663) : type code of the variable currently in DAC
//   M_DAC    (0xF7F6) : Decimal ACcumulator (16 bytes) holding the passed value
// MSXgl declares these as inline; msxmvl exposes real linkable functions so a
// single test driver can link against real MSXgl OR msxmvl.
#ifndef MSXMVL_BASIC_USR_H
#define MSXMVL_BASIC_USR_H

#include "types.h"

typedef float f32;

//=============================================================================
// DEFINES
//=============================================================================

enum BASIC_TYPE
{
	BASIC_TYPE_INT    = 2, // 2-byte integer type
	BASIC_TYPE_STRING = 3, // String type
	BASIC_TYPE_FLOAT  = 4, // Single precision real type
	BASIC_TYPE_DOUBLE = 8, // Double precision real type
};

#define M_VALTYP					0xF663
#define M_DAC						0xF7F6

#define BASIC_GET_TYPE()			(*(u8*)(M_VALTYP))
#define BASIC_GET_INT(type)			(*(type*)(M_DAC+2))
#define BASIC_GET_BYTE()			(*(i8*)(M_DAC+2))
#define BASIC_GET_WORD()			(*(i16*)(M_DAC+2))
#define BASIC_GET_FLOAT()			(*(f32*)(M_DAC))
#define BASIC_GET_STRING()			(*(const c8**)(M_DAC+1))
#define BASIC_GET_STRING_SIZE()		(*(u8*)(M_DAC))

#define BASIC_SET_BYTE(val)			(*(i16*)(M_DAC+2) = (i16)(val))
#define BASIC_SET_WORD(val)			(*(i16*)(M_DAC+2) = (i16)(val))
#define BASIC_SET_FLOAT(val)		(*(f32*)(M_DAC) = (f32)(val))
#define BASIC_SET_STRING(val, len)	(*(u8*)(M_DAC) = (u8)(len)); (*(const c8**)(M_DAC+1) = (const c8*)(val))

//=============================================================================
// FUNCTIONS
//=============================================================================

// Group: Getter --------------------------------------------------------------

// Function: Basic_GetType - Get Basic variable type (value sent by USR()).
u8 Basic_GetType(void);

// Function: Basic_GetByte - Get Basic variable as 8-bit integer.
i8 Basic_GetByte(void);

// Function: Basic_GetWord - Get Basic variable as 16-bit integer.
i16 Basic_GetWord(void);

// Function: Basic_GetFloat - Get Basic variable as 32-bit float.
f32 Basic_GetFloat(void);

// Function: Basic_GetString - Get Basic variable as string pointer.
const c8* Basic_GetString(void);

// Function: Basic_GetStringLength - Get Basic string variable length.
u8 Basic_GetStringLength(void);

// Group: Setter --------------------------------------------------------------

// Function: Basic_SetByte - Set Basic return variable as 8-bit integer.
void Basic_SetByte(i8 val);

// Function: Basic_SetWord - Set Basic return variable as 16-bit integer.
void Basic_SetWord(i16 val);

// Function: Basic_SetFloat - Set Basic return variable as 32-bit float.
void Basic_SetFloat(f32 val);

// Function: Basic_SetString - Set Basic return variable as string pointer.
void Basic_SetString(const c8* val, u8 len);

#endif // MSXMVL_BASIC_USR_H
