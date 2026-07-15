// Example: Mutex_Lock / Mutex_Gate / Mutex_Release — 8 locks in one byte.
#include "mutex.h"
volatile u8 __at(0xE000) R[8];

MUTEX_DATA();               // defines g_Mutex — required once per application

#define MTX_VRAM  3         // name your locks

void main(void)
{
	Mutex_Init();                             // all 8 locks free
	R[1] = Mutex_Gate(MTX_VRAM) ? 1 : 0;      // 1 — free

	Mutex_Lock(MTX_VRAM);
	R[2] = g_Mutex;                           // 0x08 — bit 3 set
	R[3] = Mutex_Gate(MTX_VRAM) ? 1 : 0;      // 0 — taken
	R[4] = Mutex_Gate(2) ? 1 : 0;             // 1 — other locks unaffected

	Mutex_Release(MTX_VRAM);
	Mutex_Wait(MTX_VRAM);                     // returns at once: it is free
	R[5] = g_Mutex;                           // 0x00

	R[0] = (R[1] == 1 && R[2] == 0x08 && R[3] == 0 && R[4] == 1 && R[5] == 0)
	     ? 0xA5 : 0x00;
	for (;;) {}
}
