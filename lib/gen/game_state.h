// msxmvl module: game_state -- a state stack + packed boolean flags.
//
// Two small, common game-plumbing helpers:
//   * StateStack -- a LIFO of state ids, for nested screens: push "pause" on top of
//     "play", pop back to exactly where you were. Complements fsm (a flat current
//     state) when you need a back-stack.
//   * Flags -- boolean flags packed 8 per byte, so 256 progress/switch flags fit in
//     32 bytes (cheap to keep in SRAM saves). Plain functions over a caller-owned
//     byte array; no global state.
// A game-framework convenience (Category E3).
#ifndef MSXMVL_GAME_STATE_H
#define MSXMVL_GAME_STATE_H

#include "types.h"

//=============================================================================
// State stack
//=============================================================================

#define STATE_STACK_MAX 8      // depth of the state stack
#define STATE_NONE      0xFF   // returned when the stack is empty

typedef struct {
	u8 items[STATE_STACK_MAX];
	u8 top;                     // number of items on the stack (0 = empty)
} StateStack;

// Empty the stack.
void StateStack_Init(StateStack* s);

// Push a state id. Returns 1 on success, 0 if the stack is full.
u8 StateStack_Push(StateStack* s, u8 state);

// Pop and return the top state id, or STATE_NONE if the stack is empty.
u8 StateStack_Pop(StateStack* s);

// Peek the top state id without popping, or STATE_NONE if empty.
u8 StateStack_Top(const StateStack* s);

// Number of states currently on the stack.
u8 StateStack_Count(const StateStack* s);

//=============================================================================
// Packed boolean flags (8 per byte)
//=============================================================================

// Clear all flags in a byte array of nbytes (8*nbytes flags).
void Flags_Clear(u8* flags, u8 nbytes);

// Set / reset / toggle flag `index` (bit index across the array).
void Flags_Set(u8* flags, u16 index);
void Flags_Reset(u8* flags, u16 index);
void Flags_Toggle(u8* flags, u16 index);

// Read flag `index`: returns 1 if set, 0 if clear.
u8 Flags_Get(const u8* flags, u16 index);

#endif // MSXMVL_GAME_STATE_H
