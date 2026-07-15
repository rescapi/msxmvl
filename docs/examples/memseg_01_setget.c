// memseg tracks which RAM-mapper segment is in the page-2 window (0x8000) with a
// software shadow, because the mapper port is write-only. MemSeg_Init sets a known
// "home" segment; MemSeg_SetWindow maps another; MemSeg_GetWindow reads it back.
#include "memseg.h"
volatile u8 __at(0xE000) R[4];

void main(void)
{
	MemSeg_Init(16);                    // window = segment 16
	R[1] = MemSeg_GetWindow();          // 16

	MemSeg_SetWindow(20);               // now segment 20 is in the window
	R[2] = MemSeg_GetWindow();          // 20

	R[0] = (R[1] == 16 && R[2] == 20) ? 0xA5 : 0x00;
	for (;;) {}
}
