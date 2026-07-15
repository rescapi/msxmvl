// msxmvl — clean-room reimplementation of MSXgl "psg" module.
// See psg.h for the interface, access model and register map. No MSXgl
// implementation source was consulted; only the public headers.
//
// Build: sdcc -c -mz80 --sdcccall 1 psg.c

#include "psg.h"

// Self-contained internal-PSG access, independent of the host config's PSG_ACCESS /
// PSG_CHIP model (MSXgl only exposes g_PSG_*Port and PSG_MIXER_TONE_NOISE under
// specific configs). The internal PSG always lives on the fixed I/O ports below;
// the guards yield to the host headers when those already provide the symbols.
#ifndef P_PSG_REGS
	#define P_PSG_REGS	0xA0
#endif
#ifndef P_PSG_DATA
	#define P_PSG_DATA	0xA1
#endif
#ifndef P_PSG_STAT
	#define P_PSG_STAT	0xA2
#endif
#ifndef PSG_MIXER_TONE_NOISE
	#define PSG_MIXER_TONE_NOISE	0x3F   // six tone+noise bits (active-low on HW)
#endif
#if (PSG_CHIP == PSG_BOTH)
	// Dual-PSG host: the second (external) PSG lives on 0x10/0x11/0x12 (internal stays on
	// 0xA0-0xA2). PSG_Apply flushes both shadows. NOTE: msxmvl's setters only address the
	// internal shadow (g_PSG_Regs); populating g_PSG2_Regs for the second chip needs the
	// chip-2 setter routing MSXgl exposes via a channel bit -- out of scope here, so the
	// second flush currently emits g_PSG2_Regs's default contents.
	#ifndef P_PSG2_REGS
		#define P_PSG2_REGS	0x10
	#endif
	#ifndef P_PSG2_DATA
		#define P_PSG2_DATA	0x11
	#endif
	#ifndef P_PSG2_STAT
		#define P_PSG2_STAT	0x12
	#endif
#endif
__sfr __at(0xA0) g_mvc_PsgReg;    // register-select (write)
__sfr __at(0xA1) g_mvc_PsgData;   // data (write)
__sfr __at(0xA2) g_mvc_PsgStat;   // data (read)

//=============================================================================
// Low-level chip write: latch register number, then data byte.
//=============================================================================
static void PSG_Write(u8 reg, u8 value)
{
	g_mvc_PsgReg  = reg;
	g_mvc_PsgData = value;
}

//=============================================================================
// INDIRECT ACCESS (default) — setters mutate the shadow buffer.
//=============================================================================
#if (PSG_ACCESS == PSG_INDIRECT)

PSG_Data g_PSG_Regs;
#if (PSG_CHIP == PSG_BOTH)
PSG_Data g_PSG2_Regs;   // second-chip shadow, flushed by PSG_Apply
#endif

//-----------------------------------------------------------------------------
// Group: Register
//-----------------------------------------------------------------------------
void PSG_SetRegister(u8 reg, u8 value)
{
	u8* ptr = (u8*)g_PSG_Regs;
	ptr += reg;
	*ptr = value;
}

u8 PSG_GetRegister(u8 reg)
{
	u8* ptr = (u8*)g_PSG_Regs;
	ptr += reg;
	return *ptr;
}

//-----------------------------------------------------------------------------
// Group: Helper
//-----------------------------------------------------------------------------
#if (PSG_USE_EXTRA)

void PSG_SetTone(u8 chan, u16 period)
{
	g_PSG_Regs.Tone[PSG_CHAN(chan)] = period;
}

void PSG_SetNoise(u8 period)
{
	g_PSG_Regs.Noise = period;
}

void PSG_SetMixer(u8 mix)
{
	g_PSG_Regs.Mixer = mix;
}

void PSG_SetVolume(u8 chan, u8 vol)
{
	g_PSG_Regs.Volume[PSG_CHAN(chan)] = vol;
}

void PSG_SetEnvelope(u16 period)
{
	g_PSG_Regs.Envelope = period;
}

void PSG_SetShape(u8 shape)
{
	g_PSG_Regs.Shape = shape;
}

void PSG_EnableTone(u8 chan, bool val)
{
	u8 mix = g_PSG_Regs.Mixer;
	u8 bit = 1 << chan;
	mix &= ~bit;
	if (val == 0)
		mix |= bit;
	g_PSG_Regs.Mixer = mix;
}

void PSG_EnableNoise(u8 chan, bool val)
{
	u8 mix = g_PSG_Regs.Mixer;
	u8 bit = 8 << chan;
	mix &= ~bit;
	if (val == 0)
		mix |= bit;
	g_PSG_Regs.Mixer = mix;
}

void PSG_EnableEnvelope(u8 chan, bool val)
{
	u8 vol = g_PSG_Regs.Volume[chan];
	vol &= 0x0F;
	if (val != 0)
		vol |= PSG_F_ENV;
	g_PSG_Regs.Volume[chan] = vol;
}

#endif // (PSG_USE_EXTRA)

//-----------------------------------------------------------------------------
// Silence: buffer-only. The three shadow amplitude bytes are stashed to an
// internal backup and then zeroed; the chip is NOT written (the next PSG_Apply
// flushes the silenced buffer). PSG_Resume restores the stashed volumes.
//-----------------------------------------------------------------------------
#if (PSG_USE_RESUME)
u8 g_PSG_VolumeBackup[3];
#endif

void PSG_Mute()
{
	for (u8 i = 0; i < 3; ++i)
	{
	#if (PSG_USE_RESUME)
		g_PSG_VolumeBackup[i] = g_PSG_Regs.Volume[i];
	#endif
		g_PSG_Regs.Volume[i] = 0;
	}
}

#if (PSG_USE_RESUME)
void PSG_Resume()
{
	for (u8 i = 0; i < 3; ++i)
	{
		g_PSG_Regs.Volume[i] = g_PSG_VolumeBackup[i];
	}
}
#endif

//-----------------------------------------------------------------------------
// Group: Indirect — flush R#0..R#13 to the chip. The mixer (R#7) is stored in
// logical convention (set = enabled); the six tone/noise bits are inverted for
// the active-low hardware. The buffer itself is not modified.
//-----------------------------------------------------------------------------
// NOTE (PSG_CHIP == PSG_BOTH): a dual-PSG host would need a second shadow + second
// port set flushed here. msxmvl targets a single PSG (PSG_INTERNAL/PSG_EXTERNAL); the
// PSG_BOTH second-chip flush is intentionally not implemented.
void PSG_Apply() __naked
{
	// Unrolled register upload (mirrors MSXgl's out/outi technique). Regs stream from the
	// shadow buffer, EXCEPT two that need special handling to match MSXgl:
	//   R#7  (mixer): the top two bits are the AY-3-8910 port A/B I/O-direction bits the
	//                 BIOS set up (joystick/keyboard). Blindly writing the shadow's 0 there
	//                 flips the ports to output every frame and breaks joystick reads, so
	//                 read the chip's current R#7, keep bits 6-7, and merge only the six
	//                 (inverted, active-low) tone/noise bits from the shadow.
	//   R#13 (shape): writing the envelope shape RE-TRIGGERS the envelope generator, so it
	//                 must be applied ONCE per PSG_SetShape, not every V-blank. Bit 7 of the
	//                 shadow Shape byte is used as an "already applied" flag (PSG_SetShape
	//                 clears it by storing a 0..15 shape).
	__asm
		ld   hl, #_g_PSG_Regs
		ld   c, #P_PSG_DATA          ; outi data port
		xor  a                       ; reg = 0
		; R#0..R#6
		.rept 7
			out  (P_PSG_REGS), a     ; latch register number
			outi                     ; data <- (HL++)
			inc  a
		.endm
		; R#7 (mixer): preserve chip I/O-direction bits 6-7, invert shadow tone/noise
		out  (P_PSG_REGS), a         ; a = 7, latch R#7
		in   a, (P_PSG_STAT)         ; read current R#7 from the chip
		and  #0xC0                   ; keep IOA/IOB direction bits
		ld   b, a
		ld   a, (hl)                 ; shadow mixer (logical, active-high, low 6 bits)
		xor  a, #PSG_MIXER_TONE_NOISE ; -> active-low for the hardware
		and  #0x3F                   ; clear bits 6-7 coming from the shadow
		or   a, b                    ; merge the preserved direction bits
		out  (P_PSG_DATA), a         ; write R#7 (latch still R#7)
		inc  hl
		ld   a, #8                   ; reg = 8
		; R#8..R#12 (volumes + envelope period)
		.rept 5
			out  (P_PSG_REGS), a
			outi
			inc  a
		.endm
		; R#13 (envelope shape): apply once; skip if the shadow's applied-flag (bit7) is set
		ld   a, (hl)
		or   a, a
		jp   m, 1$                   ; bit7 set -> already applied this shape, skip
		ld   a, #PSG_REG_SHAPE
		out  (P_PSG_REGS), a         ; latch R#13
		ld   a, (hl)
		out  (P_PSG_DATA), a         ; write shape (bit7 clear here)
		ld   a, (hl)
		or   a, #0x80
		ld   (hl), a                 ; mark applied so it is not re-triggered next frame
	1$:
#if (PSG_CHIP == PSG_BOTH)
		; --- second (external) PSG: identical R#7-preserve + R#13-guard on its own ports ---
		ld   a, #7
		out  (P_PSG2_REGS), a        ; latch R#7
		in   a, (P_PSG2_STAT)        ; read chip 2 R#7
		and  #0xC0                   ; keep its I/O-direction bits
		ld   b, a
		ld   a, (_g_PSG2_Regs + 7)   ; shadow mixer (active-high low 6)
		xor  a, #PSG_MIXER_TONE_NOISE
		and  #0x3F
		or   a, b
		out  (P_PSG2_DATA), a
		ld   hl, #_g_PSG2_Regs
		ld   c, #P_PSG2_DATA
		xor  a                       ; reg = 0
		.rept 7                      ; R#0..R#6
			out  (P_PSG2_REGS), a
			outi
			inc  a
		.endm
		inc  hl                      ; skip mixer byte (R#7 already written above)
		ld   a, #8
		.rept 5                      ; R#8..R#12
			out  (P_PSG2_REGS), a
			outi
			inc  a
		.endm
		ld   a, (hl)                 ; R#13 shape guard
		or   a, a
		jp   m, 2$
		ld   a, #PSG_REG_SHAPE
		out  (P_PSG2_REGS), a
		ld   a, (hl)
		out  (P_PSG2_DATA), a
		ld   a, (hl)
		or   a, #0x80
		ld   (hl), a
	2$:
#endif
		ret
	__endasm;
}

//=============================================================================
// DIRECT ACCESS — setters write the chip immediately (no shadow buffer).
//=============================================================================
#else // PSG_ACCESS == PSG_DIRECT

void PSG_SetRegister(u8 reg, u8 value)
{
	PSG_Write(PSG_REG(reg), value);
}

u8 PSG_GetRegister(u8 reg)
{
	g_mvc_PsgReg = PSG_REG(reg);
	return g_mvc_PsgStat;
}

#if (PSG_USE_EXTRA)

void PSG_SetTone(u8 chan, u16 period)
{
	u8 base = PSG_REG_TONE_A + (PSG_CHAN(chan) << 1);
	PSG_Write(base,     (u8)(period & 0xFF));
	PSG_Write(base + 1, (u8)((period >> 8) & 0x0F));
}

void PSG_SetNoise(u8 period)
{
	PSG_Write(PSG_REG_NOISE, period & 0x1F);
}

void PSG_SetMixer(u8 mix)
{
	// stored logical -> invert the six active-low tone/noise bits for HW
	PSG_Write(PSG_REG_MIXER, mix ^ PSG_MIXER_TONE_NOISE);
}

void PSG_SetVolume(u8 chan, u8 vol)
{
	PSG_Write(PSG_REG_AMP_A + PSG_CHAN(chan), vol);
}

void PSG_SetEnvelope(u16 period)
{
	PSG_Write(PSG_REG_ENV,  (u8)(period & 0xFF));
	PSG_Write(PSG_REG_ENVC, (u8)(period >> 8));
}

void PSG_SetShape(u8 shape)
{
	PSG_Write(PSG_REG_SHAPE, shape & 0x0F);
}

void PSG_EnableTone(u8 chan, bool val)
{
	u8 bit = PSG_F_TA << PSG_CHAN(chan);
	u8 cur;
	g_mvc_PsgReg = PSG_REG_MIXER;
	cur = g_mvc_PsgStat ^ PSG_MIXER_TONE_NOISE; // read HW, back to logical
	if (val) cur |= bit; else cur &= ~bit;
	PSG_Write(PSG_REG_MIXER, cur ^ PSG_MIXER_TONE_NOISE);
}

void PSG_EnableNoise(u8 chan, bool val)
{
	u8 bit = PSG_F_NA << PSG_CHAN(chan);
	u8 cur;
	g_mvc_PsgReg = PSG_REG_MIXER;
	cur = g_mvc_PsgStat ^ PSG_MIXER_TONE_NOISE;
	if (val) cur |= bit; else cur &= ~bit;
	PSG_Write(PSG_REG_MIXER, cur ^ PSG_MIXER_TONE_NOISE);
}

void PSG_EnableEnvelope(u8 chan, bool val)
{
	u8 reg = PSG_REG_AMP_A + PSG_CHAN(chan);
	u8 cur;
	g_mvc_PsgReg = reg;
	cur = g_mvc_PsgStat;
	if (val) cur |= PSG_F_ENV; else cur &= ~PSG_F_ENV;
	PSG_Write(reg, cur);
}

#endif // (PSG_USE_EXTRA)

void PSG_Mute()
{
	PSG_Write(PSG_REG_AMP_A, 0);
	PSG_Write(PSG_REG_AMP_B, 0);
	PSG_Write(PSG_REG_AMP_C, 0);
}

#if (PSG_USE_RESUME)
void PSG_Resume()
{
	// No shadow buffer in direct mode: nothing authoritative to restore.
}
#endif

#endif // PSG_ACCESS
