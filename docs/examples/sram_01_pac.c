// savegame_pac.c — save the game to a PAC cartridge: battery-backed, no disk.
//
// The PAC (Panasoft SW-M001) is a separate cartridge holding 8 KB of battery-backed
// SRAM that any game may save to. Sram_Detect scans every slot for one; the block
// calls unlock, copy and RELOCK internally, so a crash can never leave the SRAM
// writable. Sram_Verify reads the bytes back — a dead battery fails right here,
// not silently at the next power-on.
#include "sram.h"

typedef struct { u8 level; u8 lives; u16 score; u8 name[4]; } SaveGame;

// TRUE if a PAC (or FM-PAC) is present in any slot. Call once at startup.
bool save_init(void)
{
	return Sram_Detect();              // remembers the slot in g_Sram_Slot
}

// Store the save block at the start of the SRAM and prove it stuck.
bool save_store(const SaveGame* s)
{
	Sram_Write(0, s, sizeof(SaveGame));
	return Sram_Verify(0, s, sizeof(SaveGame));   // read-back = battery health
}

// Load it back (a fresh PAC reads as 0xFF bytes — check your own record).
void save_fetch(SaveGame* s)
{
	Sram_Read(0, s, sizeof(SaveGame));
}

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xE000) R[8];

// Raw interslot read (BIOS RDSLT) so the relock assert below does NOT go
// through the module under test: a locked PAC reads 0xFF at 0x4000.
static u8 raw_read(u16 addr, u16 slot) __naked
{
	(void)addr; (void)slot;
	__asm
		ld   a, e
		call 0x000C        ; RDSLT (A=slot, HL=addr) -> A
		ei
		ret
	__endasm;
}

static SaveGame g_Save = { 3, 2, 1500, "AAA" };
static SaveGame g_Back;

void main(void)
{
	u8 found, stored, fetched, locked, raw;

	found = save_init() ? 1 : 0;
	stored = save_store(&g_Save) ? 1 : 0;

	g_Back.level = 0;
	save_fetch(&g_Back);
	fetched = (g_Back.level == 3 && g_Back.lives == 2 &&
	           g_Back.score == 1500 && g_Back.name[0] == 'A') ? 1 : 0;

	// after every block call the PAC must be LOCKED again: a raw read of the
	// first SRAM byte returns 0xFF (unmapped), not the level byte we stored.
	raw = raw_read(0x4000, g_Sram_Slot);
	locked = (raw != g_Save.level) ? 1 : 0;

	R[1] = found;
	R[2] = stored;
	R[3] = fetched;
	R[4] = locked;
	R[5] = raw;                          // 0xFF when locked
	R[6] = (Sram_Size() == 8190u) ? 1 : 0;

	R[0] = (found && stored && fetched && locked && R[6]) ? 0xA5 : 0x00;
	for (;;) {}
}
