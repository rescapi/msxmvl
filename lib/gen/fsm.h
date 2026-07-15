// msxmvl clean-room reimplementation of MSXgl "fsm" module.
// Drop-in: exposes MSXgl's exact public names + signatures.
// Finite State Machine.
#ifndef MSXMVL_FSM_H
#define MSXMVL_FSM_H

#include "types.h"

//=============================================================================
// DEFINES
//=============================================================================

#ifndef MSXMVL_CALLBACK_DEFINED
#define MSXMVL_CALLBACK_DEFINED
typedef void (*callback)(void);			// Callback default signature
#endif

// State descriptor.
typedef struct FSM_State
{
	u8       Flag;					// State behavior flag (reserved)
	callback Begin;					// Function to be called when state begin (can be NULL)
	callback Update;				// Function to be called at each update tick
	callback End;					// Function to be called when state end (can be NULL)
} FSM_State;

// FSM globals.
extern FSM_State* g_CurrentState;
extern FSM_State* g_PrevState;
extern FSM_State* g_NextState;

//=============================================================================
// FUNCTIONS
//=============================================================================

// Function: FSM_SetState
// Set current state and handle transition.
// Should not be called from Begin/End callbacks.
void FSM_SetState(FSM_State* state);

// Function: FSM_GetState
// Get current state.
inline FSM_State* FSM_GetState() { return g_CurrentState; }

// Function: FSM_SetPrevious
// Reset previous state.
// Should not be called from Begin/End callbacks.
inline void FSM_SetPrevious() { FSM_SetState(g_PrevState); g_PrevState = NULL; }

// Function: FSM_GetPrevious
// Get previous state.
inline FSM_State* FSM_GetPrevious() { return g_PrevState; }

// Function: FSM_Update
// Update the current state.
void FSM_Update();

// Function: FSM_IsPending
// Is a new state pending.
inline bool FSM_IsPending() { return g_NextState != NULL; }

// Function: FSM_GetPending
// Get the pending state.
inline FSM_State* FSM_GetPending() { return g_NextState; }

#endif // MSXMVL_FSM_H
