// msxmvl EXTENSION module: moonblaster -- C wrapper over the mbplay replayer.
// The replayer itself is moonblaster_player.asm, pre-assembled into
// moonblaster_bin.h (generated + gated by tools/mb14blob.sh); this file only
// loads it and drives its original entry points. See moonblaster.h for the
// memory contract.
#include "types.h"
#include "msx-audio.h"
#include "moonblaster.h"
#include "moonblaster_bin.h"

#define MB_VAR(a)   (*(volatile u8*)(a))
#define MB_CALL(a)  mb_call((void (*)(void))(a))

// Call a replayer entry point. Every mbplay entry (strmus/stpmus/cntmus, and
// the H.TIMI hook itself) was designed to be reached from the BIOS ISR, which
// saves ALL registers first -- so the replayer clobbers IX/IY freely. SDCC uses
// IX as its frame pointer, so C code cannot survive a bare call.
static void mb_call(void (*fn)(void)) __naked
{
	(void)fn;   // in HL (sdcccall 1)
	__asm
	push ix
	push iy
	ld de, #00001$
	push de
	jp (hl)
00001$:
	pop iy
	pop ix
	ret
	__endasm;
}

void MoonBlaster_Init(void)
{
	const u8* src = MOONBLASTER_BIN;
	u8* dst = (u8*)MOONBLASTER_ORG;
	u16 n = MOONBLASTER_SIZE;
	while (n--) *dst++ = *src++;
}

void MoonBlaster_LoadKit(const u8* mbk, u16 dataLen)
{
	// MoonBlaster kits target the stock Music Module's small DRAM; the replayer
	// addresses samples in that layout, so the RAM mode must match it.
	MSXAudio_ADPCM_SetRamMode(MSXAUDIO_ADPCM_RAM_64K);
	if (dataLen == 0) dataLen = MOONBLASTER_KIT_FULL;
	MSXAudio_ADPCM_WriteMemory(0, mbk + MOONBLASTER_KIT_HEADER, dataLen);
}

void MoonBlaster_Start(const void* song, u8 chips)
{
	MB_CALL(MOONBLASTER_STPMUS);            // strmus refuses to start over a playing song
	MB_VAR(MOONBLASTER_CHIPS) = chips;
	*(volatile u16*)MOONBLASTER_MUSADR = (u16)song;
	MB_CALL(MOONBLASTER_STRMUS);
}

void MoonBlaster_Stop(void)     { MB_CALL(MOONBLASTER_STPMUS); }
void MoonBlaster_Continue(void) { MB_CALL(MOONBLASTER_CNTMUS); }

void MoonBlaster_Fade(u8 speed)
{
	MB_VAR(MOONBLASTER_FADSPD) = speed;
	MB_VAR(MOONBLASTER_FADING) = 255;
}

u8 MoonBlaster_IsPlaying(void) { return MB_VAR(MOONBLASTER_BUSPLY); }
u8 MoonBlaster_Position(void)  { return MB_VAR(MOONBLASTER_POS); }
u8 MoonBlaster_Step(void)      { return MB_VAR(MOONBLASTER_STEP); }

void MoonBlaster_Tick(void)
{
	// One interrupt's worth of playback, exactly as the BIOS would deliver it.
	// H.TIMI is normally entered from the BIOS ISR with every register already
	// saved, so the replayer clobbers freely -- including IX, SDCC's frame
	// pointer. Save what C code cannot afford to lose.
	__asm
	push ix
	push iy
	di
	call 0xFD9F
	pop iy
	pop ix
	__endasm;
}
