# Game Menu (`game_menu`)

A **data-driven menu** for title screens, option screens, pause menus — the boring
part of every game, done once. You describe the menu as data (an array of
`{label, id}` items); `game_menu` tracks the cursor, handles up/down navigation and
selection from the input module, and hands back the chosen `id`. It is
**rendering-agnostic**: it never touches the screen, so you draw the labels however
you like (text, tiles, sprites). Link `game_menu.c input.c`, include `game_menu.h`.

## API

```c
#include "game_menu.h"

void Menu_Init(Menu* m, const MenuItem* items, u8 count, u8 wrap); // set up (cursor 0)
void Menu_Move(Menu* m, i8 delta);        // move cursor (negative up); wrap or clamp
u8   Menu_CurrentId(const Menu* m);       // id of the highlighted item
u8   Menu_Update(Menu* m);                // read input; MENU_NONE or the chosen id
```

`wrap = 1` makes the cursor wrap top↔bottom; `wrap = 0` clamps at the ends.

## Usage

```c
static const MenuItem g_Items[3] = {
	{ "New Game", 10 },
	{ "Options",  20 },
	{ "Quit",     30 },
};
Menu g_Menu;

void title_screen(void) {
	Menu_Init(&g_Menu, g_Items, 3, 1);       // wrapping menu
	for (;;) {
		Joystick_Update();                   // input module: sample the pads
		u8 chosen = Menu_Update(&g_Menu);    // move on up/down, select on trigger A
		draw_menu(&g_Menu);                  // your renderer: highlight g_Menu.cursor
		if (chosen != MENU_NONE) {
			if (chosen == 30) return;        // Quit
			// ... act on New Game / Options ...
		}
	}
}
```

`Menu_Update` returns `MENU_NONE` while the player is still navigating, or the
highlighted item's `id` on the frame trigger A is pressed. The cursor index is
`g_Menu.cursor` — read it in your draw routine to highlight the current row.

## Correctness

`game_menu_01.c` exercises the navigation deterministically on hardware — wrap
(10→20→wrap→10→wrap→30) and clamp (stays at the ends) — and passes on C-BIOS MSX2.
*(tested: `game_menu_01.c`)*

## See also
- [Keyboard Input](Keyboard-Input.md) — the input module `Menu_Update` builds on.
- [State Machines](State-Machines.md) — `fsm` for screen/scene flow around the menu.
