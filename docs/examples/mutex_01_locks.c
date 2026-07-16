// sprite_lock.c — one lock that keeps the interrupt handler off the sprite table
// while the main loop is halfway through rewriting it.
//
// mutex packs eight independent binary locks into a single byte, g_Mutex. The main
// loop takes the lock around its sprite-table update; the interrupt handler gates on
// it and simply skips a frame rather than drawing from a half-written table.
#include "mutex.h"

MUTEX_DATA();               // reserves g_Mutex — once per application

#define LOCK_SPRITES  3     // name the lock instead of hard-coding bit 3

void sprites_init(void)   { Mutex_Init(); }
void sprites_lock(void)   { Mutex_Lock(LOCK_SPRITES); }
void sprites_unlock(void) { Mutex_Release(LOCK_SPRITES); }
bool sprites_free(void)   { return Mutex_Gate(LOCK_SPRITES); }

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xE000) R[8];
void main(void)
{
	sprites_init();                           // all 8 locks free
	R[1] = sprites_free() ? 1 : 0;            // 1 — free

	sprites_lock();
	R[2] = g_Mutex;                           // 0x08 — bit 3 set
	R[3] = sprites_free() ? 1 : 0;            // 0 — taken
	R[4] = Mutex_Gate(2) ? 1 : 0;             // 1 — other locks unaffected

	sprites_unlock();
	Mutex_Wait(LOCK_SPRITES);                 // returns at once: it is free
	R[5] = g_Mutex;                           // 0x00

	R[0] = (R[1] == 1 && R[2] == 0x08 && R[3] == 0 && R[4] == 1 && R[5] == 0)
	     ? 0xA5 : 0x00;
	for (;;) {}
}
