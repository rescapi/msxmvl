// enemy.c — read a monster's stats where they live, without copying them out.
//
// Sometimes you want to reach into far data and read one field, not copy the whole thing.
// A plain pointer into a bank is risky: if anything swaps the window, it points at the wrong
// memory. Far_With makes it safe — it maps the bank, hands your callback an ordinary valid
// pointer, and restores the previous bank when the callback returns. The pointer lives only
// inside the callback, so it can never dangle.
#include "farmem.h"

typedef struct { u8 hp; u8 attack; } Monster;

static u8 s_hp;
static void grab_hp(const void* mapped, void* ctx)
{
	(void)ctx;
	s_hp = ((const Monster*)mapped)->hp;    // a normal pointer — valid right here
}

// Read a monster's HP straight out of its bank (no copy of the whole struct).
u8 enemy_hp(u8 bank)
{
	Far_With(FAR(bank, 0), sizeof(Monster), grab_hp, (void*)0);
	return s_hp;
}

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xE000) R[4];
void main(void)
{
	MemSeg_Init(16);
	Far_Poke(FAR(21, 0), 100);          // put a monster with 100 HP in bank 21
	R[1] = enemy_hp(21);                // read its HP in place -> 100
	R[2] = MemSeg_GetWindow();          // 16 — Far_With put our bank back
	R[0] = (R[1] == 100 && R[2] == 16) ? 0xA5 : 0x00;
	for (;;) {}
}
