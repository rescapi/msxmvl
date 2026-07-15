// Far_With maps a segment, hands your callback a REAL pointer to the data (valid
// only inside the callback), then restores the previous segment. Because the
// pointer can't escape the callback, the "stale pointer into a paged-out segment"
// bug can't happen. Use it to work on a far struct in place.
#include "farmem.h"
volatile u8 __at(0xE000) R[4];

static u8 g_seen;
static void inspect(const void* win, void* ctx)
{
	(void)ctx;
	g_seen = *(const volatile u8*)win;      // read the mapped byte in-scope
}

void main(void)
{
	MemSeg_Init(16);
	Far_Poke(FAR(21, 0x0000), 0x99);

	Far_With(FAR(21, 0x0000), 1, inspect, (void*)0);

	R[1] = g_seen;                          // 0x99
	R[2] = MemSeg_GetWindow();              // 16 — Far_With restored home
	R[0] = (g_seen == 0x99 && R[2] == 16) ? 0xA5 : 0x00;
	for (;;) {}
}
