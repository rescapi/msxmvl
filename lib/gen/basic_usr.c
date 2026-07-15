// msxmvl clean-room reimplementation of MSXgl "basic_usr" module.
// Implemented from the module header (interface/contract) + MSX BIOS knowledge.
// SDCC --sdcccall 1.
//
// These routines are pure BIOS-variable accessors (no VRAM, no I/O). MSXgl
// declares them inline; the header has NO __PRESERVES contracts, so standard
// SDCC codegen (plain C) is used and register preservation follows the default
// SDCC calling convention. Behaviour = exact memory reads/writes of the DAC
// (0xF7F6) and VALTYP (0xF663) BASIC interchange variables.

#include "basic_usr.h"

//=============================================================================
// Getter
//=============================================================================

u8 Basic_GetType(void)
{
	return BASIC_GET_TYPE();
}

i8 Basic_GetByte(void)
{
	return BASIC_GET_BYTE();
}

i16 Basic_GetWord(void)
{
	return BASIC_GET_WORD();
}

f32 Basic_GetFloat(void)
{
	return BASIC_GET_FLOAT();
}

const c8* Basic_GetString(void)
{
	return BASIC_GET_STRING();
}

u8 Basic_GetStringLength(void)
{
	return BASIC_GET_STRING_SIZE();
}

//=============================================================================
// Setter
//=============================================================================

void Basic_SetByte(i8 val)
{
	BASIC_SET_BYTE(val);
}

void Basic_SetWord(i16 val)
{
	BASIC_SET_WORD(val);
}

void Basic_SetFloat(f32 val)
{
	BASIC_SET_FLOAT(val);
}

void Basic_SetString(const c8* val, u8 len)
{
	BASIC_SET_STRING(val, len);
}
