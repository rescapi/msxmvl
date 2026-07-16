// hero_fsm.c — the player's own state machine: stand idle, then jump.
//
// Each state is three optional callbacks: Begin runs once as you enter it, Update
// runs every frame, End runs once as you leave. FSM_SetState only *queues* the
// change — the real switch (old End, then new Begin) happens inside the next
// FSM_Update, so it is safe to ask for a new state from inside an Update.
#include "fsm.h"

static u8 g_IdleFrames;    // idle animation cursor: frames spent standing
static u8 g_JumpsStarted;  // how many times the hero launched a jump
static u8 g_IdleExits;     // how many times the hero left the idle state

static void idle_update(void) { g_IdleFrames++; }    // advance the idle animation
static void idle_leave(void)  { g_IdleExits++; }     // clean up as we leave idle
static void jump_launch(void) { g_JumpsStarted++; }  // kick off the jump arc
static void jump_update(void) { }                    // airborne this frame

//                         Flag  Begin        Update       End
static FSM_State g_Idle = {  0,  NULL,        idle_update, idle_leave };
static FSM_State g_Jump = {  0,  jump_launch, jump_update, NULL       };

void hero_init(void)       { FSM_SetState(&g_Idle); }
void hero_jump(void)       { FSM_SetState(&g_Jump); }
void hero_tick(void)       { FSM_Update(); }
bool hero_is_jumping(void) { return FSM_GetState() == &g_Jump; }

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xE000) R[8];
void main(void)
{
	hero_init();                          // queue the idle state
	R[1] = FSM_IsPending() ? 1 : 0;             // 1 — not applied yet

	hero_tick();                          // applies Idle, then ticks it
	hero_tick();
	R[2] = g_IdleFrames;                        // 2

	hero_jump();                          // queue the jump
	hero_tick();                          // Idle.End, Jump.Begin, Jump.Update
	R[3] = g_IdleExits;                         // 1
	R[4] = g_JumpsStarted;                      // 1
	R[5] = hero_is_jumping() ? 1 : 0;           // 1

	R[0] = (R[1] == 1 && R[2] == 2 && R[3] == 1 && R[4] == 1 && R[5] == 1)
	     ? 0xA5 : 0x00;
	for (;;) {}
}
