// render.c — manually select which page the VDP scans out.
//
// Display_ShowPage(p) is the low-level control that makes the VDP display page p — it writes
// VDP register R#2 = (p<<5) | 0x1F. Init and Flip use it internally; call it yourself only for
// manual page control, such as parking the display on a static full-screen image. It does not
// touch the front/back double-buffer bookkeeping.
#include "display.h"

#define PAGE_TITLE  3      // the page holding a full-screen title image

// Park the display on the title page.
void show_title_screen(void)
{
	Display_ShowPage(PAGE_TITLE);
}

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xE000) R[2];
void main(void)
{
	show_title_screen();           // R#2 = (3<<5)|0x1F = 0x7F
	R[0] = 0xA5;                   // reached here without hanging; R#2 checked by harness
	for (;;) {}
}
