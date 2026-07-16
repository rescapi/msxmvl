// render.c — flip the freshly drawn page onto the screen.
//
// Display_Flip swaps the front and back pages: whatever you just drew into the hidden page
// becomes visible, and the page that was on screen becomes your next hidden draw target. Two
// flips bring you back to where you started. It does not wait for vblank — call
// VDP_WaitVBlank() first for a tear-free flip.
#include "display.h"

// Reveal the frame we just drew; the old screen becomes the next draw target.
void present_frame(void)
{
	Display_Flip();
}

u8 visible_page(void) { return Display_GetFrontPage(); }   // page on screen
u8 draw_page(void)    { return Display_GetBackPage(); }    // hidden page, draw here

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xE000) R[4];
void main(void)
{
	Display_InitDoubleBuffer();    // front=0 back=1
	present_frame();               // flip -> front=1 back=0
	R[1] = visible_page();         // 1
	R[2] = draw_page();            // 0

	present_frame();               // flip -> front=0 back=1 again
	R[3] = visible_page();         // 0

	R[0] = (R[1] == 1 && R[2] == 0 && R[3] == 0) ? 0xA5 : 0x00;
	for (;;) {}
}
