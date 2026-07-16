// render.c — set up double buffering and find the hidden page to draw into.
//
// With two VRAM pages we show one while drawing the next, then flip — no flicker. After
// Display_InitDoubleBuffer, page 0 is on screen (the "front") and page 1 is hidden (the
// "back", your draw target). Every frame you add Display_GetBackPageY() to a VDP command's
// destination Y so the draw lands in the hidden page instead of the visible one.
#include "display.h"

// Bring up double buffering: show page 0, draw into page 1.
void render_init(void)
{
	Display_InitDoubleBuffer();
}

u8  visible_page(void) { return Display_GetFrontPage(); }   // page on screen
u8  draw_page(void)    { return Display_GetBackPage(); }    // hidden page, draw here
u16 draw_page_y(void)  { return Display_GetBackPageY(); }   // Y offset of the hidden page

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xE000) R[4];
void main(void)
{
	render_init();

	R[1] = visible_page();                  // 0 (shown)
	R[2] = draw_page();                     // 1 (hidden, draw here)
	R[3] = (u8)(draw_page_y() >> 8);        // 256>>8 = 1

	R[0] = (visible_page() == 0 &&
	        draw_page()    == 1 &&
	        draw_page_y()  == 256) ? 0xA5 : 0x00;
	for (;;) {}
}
