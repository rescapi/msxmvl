// Far_Set fills a far region with one byte (boundary auto-split) — use it to clear
// or initialize a multi-segment scratch buffer before use.
#include "farmem.h"
volatile u8 __at(0xE000) R[4];

void main(void)
{
	MemSeg_Init(16);

	Far_Set(FAR(22, 0x3FFE), 0xEE, 4);      // 4 bytes spanning segment 22 -> 23

	R[1] = Far_Peek(FAR(22, 0x3FFF));       // 0xEE (in seg 22)
	R[2] = Far_Peek(FAR(23, 0x0000));       // 0xEE (in seg 23)
	R[0] = (R[1] == 0xEE && R[2] == 0xEE) ? 0xA5 : 0x00;
	for (;;) {}
}
