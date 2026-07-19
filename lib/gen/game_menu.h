// msxmvl module: game_menu -- data-driven menu (items, cursor, navigation, select).
//
// Rendering-agnostic: the menu tracks the highlighted item and reports the chosen
// id; the caller draws the labels however it likes (print/tile/vdp). Navigation is
// built on the input module's joystick+cursor reads. A game-framework convenience
// (Category E1), independent of fsm/game_pawn.
#ifndef MSXMVL_GAME_MENU_H
#define MSXMVL_GAME_MENU_H

#include "types.h"

#define MENU_NONE 0xFF   // Menu_Update: nothing chosen this frame

typedef struct {
	const c8* label;     // display text (drawn by the caller)
	u8        id;        // value returned when this item is chosen
} MenuItem;

typedef struct {
	const MenuItem* items;
	u8  count;
	u8  cursor;          // index of the highlighted item
	u8  wrap;            // 1: cursor wraps top<->bottom; 0: clamps at the ends
} Menu;

// Set up a menu over items[count]; cursor starts at 0.
void Menu_Init(Menu* m, const MenuItem* items, u8 count, u8 wrap);

// Move the cursor by delta (negative = up, positive = down); honours wrap/clamp.
void Menu_Move(Menu* m, i8 delta);

// Id of the currently highlighted item (MENU_NONE if the menu is empty).
u8 Menu_CurrentId(const Menu* m);

// Read joystick/cursor input once per frame (call Joystick_Update() first): move on
// an up/down edge, return the highlighted id on a trigger-A press, else MENU_NONE.
u8 Menu_Update(Menu* m);

#endif // MSXMVL_GAME_MENU_H
