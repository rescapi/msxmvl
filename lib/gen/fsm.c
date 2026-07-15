// msxmvl "fsm" module — ORIGINAL implementation.
//
// Clean-room: written from fsm.h's interface and the module's documented behaviour. No
// MSXgl implementation source was consulted.
//
// The whole design is one idea: FSM_SetState does NOT switch. It queues. The switch happens
// inside the next FSM_Update, which runs End on the state being left, Begin on the state
// being entered, and then Update on the new state. That is what makes it safe to call
// FSM_SetState from inside an Update callback -- the rest of that frame still runs against
// a consistent current state.
//
// Do not call FSM_SetState from a Begin or End callback: that queues a transition from
// inside a transition, and the state you just entered would be replaced before it ever ran.
#include "fsm.h"

//=============================================================================
// GLOBALS
//=============================================================================

FSM_State* g_CurrentState = NULL;   // the state being updated
FSM_State* g_PrevState    = NULL;   // the state we came from (for FSM_SetPrevious)
FSM_State* g_NextState    = NULL;   // queued by FSM_SetState, consumed by FSM_Update

//=============================================================================
// FUNCTIONS
//=============================================================================

// Queue a transition. Applied by the next FSM_Update, not here.
void FSM_SetState(FSM_State* state)
{
	g_NextState = state;
}

// Apply any queued transition, then tick the current state.
void FSM_Update(void)
{
	if (g_NextState)
	{
		// Leave the old state. Begin/End are optional -- a state with no entry or exit
		// work leaves them NULL, so both are guarded.
		if (g_CurrentState && g_CurrentState->End)
			g_CurrentState->End();

		g_PrevState    = g_CurrentState;
		g_CurrentState = g_NextState;
		g_NextState    = NULL;          // cleared BEFORE Begin runs, so a Begin callback
		                                // cannot see a stale pending state

		if (g_CurrentState->Begin)
			g_CurrentState->Begin();
	}

	// Update is NOT guarded, by contract: every state must have one. A state with nothing
	// to do per tick supplies an empty function -- that is cheaper than testing a pointer
	// on every frame of every state for the lifetime of the program.
	g_CurrentState->Update();
}
