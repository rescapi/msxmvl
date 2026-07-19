// msxmvl module: game_menu -- data-driven menu. See game_menu.h.
#include "game_menu.h"
#include "input.h"

void Menu_Init(Menu* m, const MenuItem* items, u8 count, u8 wrap)
{
	m->items = items;
	m->count = count;
	m->cursor = 0;
	m->wrap = wrap;
}

void Menu_Move(Menu* m, i8 delta)
{
	i8 c;
	if (m->count == 0)
		return;
	c = (i8)m->cursor + delta;
	if (m->wrap) {
		while (c < 0)            c += (i8)m->count;
		while (c >= (i8)m->count) c -= (i8)m->count;
	} else {
		if (c < 0)               c = 0;
		if (c >= (i8)m->count)   c = (i8)m->count - 1;
	}
	m->cursor = (u8)c;
}

u8 Menu_CurrentId(const Menu* m)
{
	return m->count ? m->items[m->cursor].id : MENU_NONE;
}

u8 Menu_Update(Menu* m)
{
	u8 chg = Joystick_GetDirectionChange(JOY_PORT_1);   // edge-detected direction
	if (chg & JOY_INPUT_DIR_UP)
		Menu_Move(m, -1);
	if (chg & JOY_INPUT_DIR_DOWN)
		Menu_Move(m, +1);
	if (Joystick_IsButtonPushed(JOY_PORT_1, JOY_INPUT_TRIGGER_A))
		return Menu_CurrentId(m);
	return MENU_NONE;
}
