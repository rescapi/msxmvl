// Double-buffer bookkeeping: after init, page 0 is shown and page 1 is hidden.
// Display_GetBackPageY() gives the Y offset to add to a VDP command's DY so the
// draw lands in the hidden page (page 1 -> Y 256).
#include "display.h"
volatile u8 __at(0xE000) R[4];

void main(void)
{
	Display_InitDoubleBuffer();

	R[1] = Display_GetFrontPage();          // 0 (shown)
	R[2] = Display_GetBackPage();           // 1 (hidden, draw here)
	R[3] = (u8)(Display_GetBackPageY() >> 8);   // 256>>8 = 1

	R[0] = (Display_GetFrontPage() == 0 &&
	        Display_GetBackPage()  == 1 &&
	        Display_GetBackPageY() == 256) ? 0xA5 : 0x00;
	for (;;) {}
}
