// title_menu.c -- a title-screen menu with wrap-around navigation.
//
// A game menu is data: an array of {label, id} items. game_menu tracks the cursor
// and hands back the chosen id; you draw the labels and act on the selection. Each
// frame you'd call Joystick_Update() then Menu_Update(&menu); here we exercise the
// navigation logic directly so the behaviour is deterministic and self-checking.
#include "game_menu.h"

static const MenuItem g_Items[3] = {
	{ "New Game", 10 },
	{ "Options",  20 },
	{ "Quit",     30 },
};

// ---- test harness --------------------------------------------------------
volatile u8 __at(0xE000) R[8];
void main(void)
{
	Menu wrap, clamp;

	Menu_Init(&wrap, g_Items, 3, 1);        // wrapping menu, cursor at "New Game"
	R[1] = Menu_CurrentId(&wrap);           // 10
	Menu_Move(&wrap, +1);
	R[2] = Menu_CurrentId(&wrap);           // 20 (Options)
	Menu_Move(&wrap, +2);                   // 1+2 = 3 -> wraps to 0
	R[3] = Menu_CurrentId(&wrap);           // 10
	Menu_Move(&wrap, -1);                   // wraps to bottom
	R[4] = Menu_CurrentId(&wrap);           // 30 (Quit)

	Menu_Init(&clamp, g_Items, 3, 0);       // clamping menu (no wrap)
	Menu_Move(&clamp, -5);                  // clamps at top
	R[5] = Menu_CurrentId(&clamp);          // 10
	Menu_Move(&clamp, +9);                  // clamps at bottom
	R[6] = Menu_CurrentId(&clamp);          // 30

	R[0] = (R[1] == 10 && R[2] == 20 && R[3] == 10 &&
	        R[4] == 30 && R[5] == 10 && R[6] == 30) ? 0xA5 : 0x00;
	for (;;) {}
}
