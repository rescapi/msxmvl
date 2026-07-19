// msxmvl module: game_state -- state stack + packed flags. See game_state.h.
#include "game_state.h"

//----------------------------------------------------------------------------
// State stack
//----------------------------------------------------------------------------

void StateStack_Init(StateStack* s)
{
	s->top = 0;
}

u8 StateStack_Push(StateStack* s, u8 state)
{
	if (s->top >= STATE_STACK_MAX)
		return 0;
	s->items[s->top++] = state;
	return 1;
}

u8 StateStack_Pop(StateStack* s)
{
	if (s->top == 0)
		return STATE_NONE;
	return s->items[--s->top];
}

u8 StateStack_Top(const StateStack* s)
{
	return s->top ? s->items[s->top - 1] : STATE_NONE;
}

u8 StateStack_Count(const StateStack* s)
{
	return s->top;
}

//----------------------------------------------------------------------------
// Packed boolean flags (8 per byte): byte = index>>3, bit mask = 1<<(index&7)
//----------------------------------------------------------------------------

void Flags_Clear(u8* flags, u8 nbytes)
{
	u8 i;
	for (i = 0; i < nbytes; i++)
		flags[i] = 0;
}

void Flags_Set(u8* flags, u16 index)
{
	flags[index >> 3] |= (u8)(1 << (index & 7));
}

void Flags_Reset(u8* flags, u16 index)
{
	flags[index >> 3] &= (u8)~(1 << (index & 7));
}

void Flags_Toggle(u8* flags, u16 index)
{
	flags[index >> 3] ^= (u8)(1 << (index & 7));
}

u8 Flags_Get(const u8* flags, u16 index)
{
	return (flags[index >> 3] >> (index & 7)) & 1;
}
