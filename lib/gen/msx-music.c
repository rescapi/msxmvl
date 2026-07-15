// msxmvl — clean-room reimplementation of MSXgl "msx-music" module (bodies).
// Signatures + documented behaviour from MSXgl engine/src/msx-music.h,
// msx-music_reg.h and system_port.h ONLY. No MSXgl implementation source read.
//
// The YM2413 (OPLL) is write-only through ports 0x7C (index) / 0x7D (data).
// A RAM shadow of every written register is kept so GetRegister can return a
// value and Resume can re-play the state after a Mute.
#include "msx-music.h"
#include "bios.h"

// Sys_CheckSlot lives in the system module. It is forward-declared here rather
// than via #include "system.h" because bios.h defines g_EXPTBL as a macro while
// system.h declares it as a typed symbol — including both breaks compilation.
typedef bool (*CheckSlotCallback)(u8);
u8 Sys_CheckSlot(CheckSlotCallback cb);

//=============================================================================
// MODULE STATE
//=============================================================================

// Chip signature bytes: "APRL" (internal MSX-Music), "OPLL" (external FM-PAC).
const c8 g_MSXMusic_Ident[] = "APRLOPLL";

// Slot ID of the first MSX-Music chip found (SLOT_NOTFOUND until detected).
u8 g_MSXMusic_SlotId = SLOT_NOTFOUND;

// RAM shadow of the write-only OPLL registers (0x00..0x38 inclusive).
static u8 g_MSXMusic_Buffer[MSXMUSIC_REG_MAX + 1];

#if (MSXMUSIC_USE_RESUME)
// Register backup buffer cleared by MSXMusic_Initialize — declared here to match
// MSXgl's exact byte-for-byte Initialize (Mem_Set(0, g_MSXMusic_RegBackup, 16)).
// msxmvl's Set/Mute/Resume use g_MSXMusic_Buffer instead, so this buffer is only
// zeroed at init and otherwise unused (kept purely for MSXgl byte-parity).
u8 g_MSXMusic_RegBackup[16];
#endif

//=============================================================================
// INTERNAL HELPERS
//=============================================================================

// Write a value to an OPLL register on the chip only (no shadow update).
// OPLL wants the index on port 0x7C then the data on port 0x7D; the intervening
// SDCC instructions provide the small settle delay the chip needs.
// TIMING INVARIANT -- READ BEFORE "OPTIMISING" THIS.
//
// The YM2413 (OPLL) datasheet requires a minimum gap between chip accesses:
//     >= 12 master-clock cycles after writing the ADDRESS, before writing the data
//     >= 84 master-clock cycles after writing the DATA, before the next access
// On MSX the OPLL master clock IS the Z80 clock, so those are 12 T and 84 T.
// "If the wait time is not observed data will be uncertain." -- YM2413 Application Manual
//
// We satisfy both WITHOUT burning a single wait cycle, and it is not an accident:
//
//   address -> data:  the `ld a,c` that loads the value we are about to write sits between
//                     the two OUTs. 4 T + OUT 11 T = 15 T >= 12. Real work fills the gap.
//
//   data -> next:     every bulk path (MSXMusic_Resume, instrument upload) reaches this
//                     function through a CALL. ret + loop + call + prologue is comfortably
//                     over 84 T.
//
// So THE CALL OVERHEAD IS LOAD-BEARING. If you inline this into a tight OUT loop -- the
// obvious "optimisation", and exactly what a C-to-asm sweep would do -- the 84 T gap
// collapses and the chip latches garbage.
//
// openMSX will NOT catch it: the emulator is a correctness oracle, not a timing oracle.
// Every sound test we have passes either way. Verify against a real FM-PAC, or count the
// T-states, before changing the shape of this.
static void MSXMusic_WriteChip(u8 reg, u8 value)
{
	g_MSXMusic_IndexPort = reg;
	g_MSXMusic_DataPort = value;
}

// Sys_CheckSlot callback: does the given slot hold the internal MSX-Music
// signature ("APRL" at 0x4018)? (byte-identical to MSXgl)
bool MSXMusic_CheckInternal(u8 slotId)
{
	const c8* ptr = g_MSXMusic_Ident;
	u16 dest = 0x4018;
	while (*ptr != 0)
	{
		if (*ptr != BIOS_InterSlotRead(slotId, dest++))
			return FALSE;
		ptr++;
	}
	return TRUE;
}

// Sys_CheckSlot callback: does the given slot hold the external FM-PAC
// signature ("OPLL" at 0x401C)? (byte-identical to MSXgl)
bool MSXMusic_CheckExternal(u8 slotId)
{
	const c8* ptr = g_MSXMusic_Ident + 4;
	u16 dest = 0x401C;
	while (*ptr != 0)
	{
		if (*ptr != BIOS_InterSlotRead(slotId, dest++))
			return FALSE;
		ptr++;
	}
	return TRUE;
}

//=============================================================================
// PUBLIC API
//=============================================================================

// Search for MSX-Music (YM2413) chip (byte-identical to MSXgl): scan slots for
// the internal signature, then the external FM-PAC signature (activating it).
u8 MSXMusic_Detect()
{
	// Check internal MSX-Music chip
	g_MSXMusic_SlotId = Sys_CheckSlot(MSXMusic_CheckInternal);
	if (g_MSXMusic_SlotId != SLOT_NOTFOUND)
		return MSXMUSIC_INTERNAL;

	// Check external MSX-Music chip
	g_MSXMusic_SlotId = Sys_CheckSlot(MSXMusic_CheckExternal);
	if (g_MSXMusic_SlotId != SLOT_NOTFOUND)
	{
		// Activate external FM-PAC
		u8 val = BIOS_InterSlotRead(g_MSXMusic_SlotId, 0x7FF6);
		BIOS_InterSlotWrite(g_MSXMusic_SlotId, 0x7FF6, val | 0x01);
		return MSXMUSIC_EXTERNAL;
	}

	return MSXMUSIC_NOTFOUND;
}

// Initialize the module (byte-identical to MSXgl): detect the chip and clear the
// register backup buffer.
u8 MSXMusic_Initialize()
{
	u8 ret = MSXMusic_Detect();
	#if (MSXMUSIC_USE_RESUME)
	__builtin_memset(g_MSXMusic_RegBackup, 0, 16);	// = MSXgl's Mem_Set macro
	#endif
	return ret;
}

// Write a register value both to the shadow and to the chip.
void MSXMusic_SetRegister(u8 reg, u8 value)
{
	if (reg <= MSXMUSIC_REG_MAX)
		g_MSXMusic_Buffer[reg] = value;

	g_MSXMusic_IndexPort = reg;
	g_MSXMusic_DataPort = value;
}

// Select the register on the index port, then read the data port back.
// The OPLL is write-only, so on real hardware this returns open-bus; MSXgl
// exposes it as-is (no RAM shadow) — behaviourally GetRegister == read 0x7D.
u8 MSXMusic_GetRegister(u8 reg)
{
	g_MSXMusic_IndexPort = reg;
	return g_MSXMusic_DataPort;
}

// Silence all nine channels (max attenuation) and switch rhythm mode off.
// The shadow is left untouched so MSXMusic_Resume can restore the state.
void MSXMusic_Mute()
{
	u8 r;
	for (r = MSXMUSIC_REG_INST_1; r <= MSXMUSIC_REG_MAX; r++)
		MSXMusic_WriteChip(r, 0x0F);		// instrument 0, volume = min

	MSXMusic_WriteChip(MSXMUSIC_REG_RHYTHM, 0x00);
}

#if (MSXMUSIC_USE_RESUME)
// Undo a previous Mute by re-playing only the registers Mute actually changed.
// MSXMusic_SetRegister keeps every register's shadow and the live chip in lock-
// step, so the chip already mirrors g_MSXMusic_Buffer everywhere EXCEPT the ten
// registers Mute overwrote directly (rhythm R#0x0E and the nine channel
// instrument/volume registers R#0x30-0x38). Restoring just those reproduces the
// exact chip state of the former full 0x00-0x38 replay at a fraction of the cost
// (the rest were redundant same-value writes / writes to undefined registers).
void MSXMusic_Resume()
{
	u8 r;
	MSXMusic_WriteChip(MSXMUSIC_REG_RHYTHM, g_MSXMusic_Buffer[MSXMUSIC_REG_RHYTHM]);
	for (r = MSXMUSIC_REG_INST_1; r <= MSXMUSIC_REG_MAX; r++)
		MSXMusic_WriteChip(r, g_MSXMusic_Buffer[r]);
}
#endif
