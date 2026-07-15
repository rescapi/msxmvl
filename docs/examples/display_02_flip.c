// Display_Flip swaps front<->back: what you drew becomes visible, and the old
// visible page becomes the new hidden draw target. Two flips return to the start.
#include "display.h"
volatile u8 __at(0xE000) R[4];

void main(void)
{
	Display_InitDoubleBuffer();    // front=0 back=1
	Display_Flip();                // front=1 back=0
	R[1] = Display_GetFrontPage(); // 1
	R[2] = Display_GetBackPage();  // 0

	Display_Flip();                // front=0 back=1 again
	R[3] = Display_GetFrontPage(); // 0

	R[0] = (R[1] == 1 && R[2] == 0 && R[3] == 0) ? 0xA5 : 0x00;
	for (;;) {}
}
