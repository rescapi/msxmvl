// Example: FSM_SetState / FSM_Update — a title -> game state transition.
#include "fsm.h"
volatile u8 __at(0xE000) R[8];

static u8 g_TitleTicks, g_GameEnters, g_TitleLeft;

static void Title_Update(void) { g_TitleTicks++; }
static void Title_End(void)    { g_TitleLeft++; }
static void Game_Begin(void)   { g_GameEnters++; }
static void Game_Update(void)  { }

//                       Flag  Begin       Update        End
static FSM_State g_Title = { 0, NULL,       Title_Update, Title_End };
static FSM_State g_Game  = { 0, Game_Begin, Game_Update,  NULL      };

void main(void)
{
	FSM_SetState(&g_Title);   // queued, not applied yet
	R[1] = FSM_IsPending() ? 1 : 0;             // 1

	FSM_Update();             // applies Title, then ticks it
	FSM_Update();
	R[2] = g_TitleTicks;                        // 2

	FSM_SetState(&g_Game);    // queue the transition
	FSM_Update();             // Title.End, Game.Begin, Game.Update
	R[3] = g_TitleLeft;                         // 1
	R[4] = g_GameEnters;                        // 1
	R[5] = (FSM_GetState() == &g_Game) ? 1 : 0; // 1

	R[0] = (R[1] == 1 && R[2] == 2 && R[3] == 1 && R[4] == 1 && R[5] == 1)
	     ? 0xA5 : 0x00;
	for (;;) {}
}
