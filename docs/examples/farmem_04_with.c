// FAR POINTERS — reach into far data in place, safely.
//
// Copying data out (Far_Read) is one option; sometimes you'd rather reach into it where
// it lives and read a field directly. But a plain pointer into a segment is risky: the
// instant anything else swaps the window, that pointer is aimed at the wrong memory.
//
// Far_With makes that safe. You hand it a far pointer and a small callback; it maps the
// segment in, gives your callback an ordinary, valid pointer to the data, and — when the
// callback returns — restores whatever segment was there before. The pointer exists ONLY
// inside the callback, so it can never be left dangling.
//
// Here: a monster's stats live in far RAM. We reach in and read its HP field directly,
// without copying the whole struct out first.
#include "farmem.h"
volatile u8 __at(0xE000) R[4];

#define SEG_ENEMIES  21     // segment holding the enemy stat tables

typedef struct { u8 hp; u8 attack; } Monster;

static u8 g_hp;                                     // the callback leaves what it read here
static void read_hp(const void* mapped, void* ctx)
{
	(void)ctx;
	const Monster* m = (const Monster*)mapped;      // a normal pointer — valid right here
	g_hp = m->hp;                                   // read the HP field in place
}

void main(void)
{
	MemSeg_Init(16);
	Far_Poke(FAR(SEG_ENEMIES, 0), 100);             // give this monster 100 HP (the hp field)

	Far_With(FAR(SEG_ENEMIES, 0), sizeof(Monster), read_hp, (void*)0);   // read it in place

	R[1] = g_hp;                                    // 100 — pulled straight from far RAM
	R[2] = MemSeg_GetWindow();                      // 16 — Far_With put our segment back
	R[0] = (g_hp == 100 && R[2] == 16) ? 0xA5 : 0x00;
	for (;;) {}
}
