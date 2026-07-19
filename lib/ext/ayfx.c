// =============================================================================
// msxmvl module: ayfx -- PSG sound-effects replayer.  See ayfx.h.
//
// PROVENANCE: this is NOT clean-room. It is SapphiRe's ayFX REPLAYER (fixed-volume
// v1.31) as adapted to SDCC by mvac7 (fR3eL project, ayFXplayer v1.1, MIT), further
// altered for msxmvl. Original: <http://www.z80st.es/blog/tags/ayfx>. Redistributed
// under the original MIT terms with attribution (NOTICE.md).
//
// msxmvl ALTERATIONS (all so it becomes a drop-in SFX layer over msxmvl's PSG driver):
//   1. Shares msxmvl's PSG shadow: every `_AYREGS` reference now targets `_g_PSG_Regs`
//      (psg.h). Tone/volume/noise use the identical R#0..R#15 layout, so they need no
//      change -- SFX and music mix in one buffer flushed by the one PSG_Apply().
//   2. Mixer polarity: msxmvl's shadow mixer is active-HIGH (set bit = channel enabled)
//      whereas the original writes the raw active-LOW hardware value. The _SETMIXER
//      routine now converts to active-high for this channel's two bits, leaving the
//      other channels' (music's) bits untouched. See the marked block below.
//   3. ayFX_Setup no longer calls AY_Init (which would wipe the shared shadow and
//      silence the music); it only arms the replayer.
//   4. The three arg-taking __naked functions are marked __sdcccall(0): their hand-asm
//      reads args off the stack (IX+4..), but msxmvl builds --sdcccall 1 (register
//      args), so caller and callee must be pinned to the stack convention.
// The decode/stream-format logic is otherwise SapphiRe/mvac7's, unchanged.
// =============================================================================

#include "psg.h"
#include "ayfx.h"

u16 ayFX_BANK;      // Current ayFX bank
u8  ayFX_MODE;      // ayFX mode (0 fixed; 1 cyclic)
u8  ayFX_CHANNEL;   // internal channel counter (1=C; 2=B; 3=A)
u8  ayFX_PRIORITY;  // priority of the effect currently playing (255 = none)

u16 ayFX_POINTER;   // pointer into the current stream
u16 ayFX_TONE;      // current tone of the effect
u8  ayFX_NOISE;     // current noise of the effect
u8  ayFX_VOLUME;    // current volume of the effect

// -----------------------------------------------------------------------------
void ayFX_Setup(u16 bank, u8 mode, u8 channel) __naked __sdcccall(0)
{
bank; mode; channel;
__asm
    push  IX
    ld    IX,#0
    add   IX,SP
    ; msxmvl: do NOT clear the PSG shadow here -- music must keep playing.
    ld   L,4(IX)
    ld   H,5(IX)

    ld   A,#3
    SUB  7(IX)                  ; reverse the channel order (0/1/2 -> 3/2/1)
    ld	(#_ayFX_CHANNEL),A

    ld	(#_ayFX_BANK),HL		; current ayFX bank
    ld   A,6(IX)
    ld	(#_ayFX_MODE),A			; playback mode

ayFX_END:
    ld	a,#255				    ; lowest priority = "nothing playing"
    ld	(#_ayFX_PRIORITY),A

    pop  IX
    ret
__endasm;
}

// -----------------------------------------------------------------------------
void ayFX_SetChannel(u8 channel) __naked __sdcccall(0)
{
channel;
__asm
    push  IX
    ld    IX,#0
    add   IX,SP

    ld   A,#3
    SUB  7(IX)                  ; reverse the channel order
    ld	(#_ayFX_CHANNEL),A

    pop  IX
    ret
__endasm;
}

// -----------------------------------------------------------------------------
u8 ayFX_Play(u8 fx, u8 priority) __naked __sdcccall(0)
{
fx; priority;
__asm
    push  IX
    ld    IX,#0
    add   IX,SP

    ld    A,4(IX)               ; effect index
    ld    C,5(IX)               ; priority

    ld	b,a						; b := new effect index
    ld	hl,(#_ayFX_BANK)
    ld	a,(hl)					; number of samples in the bank
    or	a						; 0 means 256 samples
    jr	z,_afx_CHECK_PRI
    ld	a,b
    cp	(hl)					; index out of range?
    ld	a,#2
    jr	nc,_afx_INIT_END		; -> error 2

_afx_CHECK_PRI:                 ; 0 = highest priority, 15 = lowest
    ld	a,b
    ld	a,(_ayFX_PRIORITY)
    cp	c						; new priority lower than current?
    ld	a,#1
    jr	c,_afx_INIT_END			; -> error 1
    ld	a,c
    and	#0x0F
    ld	(_ayFX_PRIORITY),A		; store new priority

    ld	de,(_ayFX_BANK)
    inc	de						; -> offset table
    ld	l,b
    ld	h,#0
    add	hl,hl					; index*2
    add	hl,de
    ld	E,(HL)
    inc	hl
    ld	d,(HL)					; de := stream increment
    add	hl,de					; hl -> new stream
    ld	(_ayFX_POINTER),HL
    xor	a						; a := 0 (no error)

_afx_INIT_END:
    ld   L,A                    ; return code (SDCC sdcccall 0: in L)
    pop  IX
    ret
__endasm;
}

// -----------------------------------------------------------------------------
void ayFX_Decode(void) __naked
{
__asm
    push  IX

    ld	a,(#_ayFX_PRIORITY)
    or	a						; bit7 set (=255) -> nothing playing
    jp	M,_afx_END_decode

    ld	a,(#_ayFX_MODE)
    and	#1						; fixed channel?
    jr	z,_afx_TAKECB

    ld	hl,#_ayFX_CHANNEL		; cyclic: advance channel
    dec	(HL)
    jr	nz,_afx_TAKECB
    ld	(HL),#3

_afx_TAKECB:                    ; --- control byte ---
    ld	hl,(#_ayFX_POINTER)
    ld	c,(HL)
    inc	hl
    bit	5,c						; new tone?
    jr	z,_afx_CHECK_NN
    ld	e,(HL)
    inc	hl
    ld	d,(HL)
    inc	hl
    ld	(#_ayFX_TONE),de

_afx_CHECK_NN:                  ; new noise?
    bit	6,c
    jr	z,_afx_SETPOINTER
    ld	a,(HL)
    inc	HL
    cp	#0x20					; 0x20 marks end-of-stream
    jp	Z,_afx_STREAM_END
    ld	(#_ayFX_NOISE),A

_afx_SETPOINTER:
    ld	(#_ayFX_POINTER),HL
    ld	a,c
    and	#0x0F					; volume (low nibble)
    ld	(#_ayFX_VOLUME),A
    jr	z,_afx_END_decode		; zero volume -> leave the shadow alone

    ; --- copy the effect into the shared shadow (g_PSG_Regs) ---
    bit	7,c						; noise off?
    jr	nz,_afx_SETMASKS
    ld	a,(#_ayFX_NOISE)
    ld	(#_g_PSG_Regs+6),A		; R#6 noise period

_afx_SETMASKS:
    ld	A,C
    and	#0x90					; bits 7,4 (noise+tone enable in the control byte)
    cp	#0x90					; both off?
    jr	z,_afx_END_decode
    rrca
    rrca						; -> the channel-C mask position
    ld	d,#0xDB					; AND mask (clears this channel's two mixer bits)

    ld	hl,#_ayFX_CHANNEL
    ld	b,(HL)					; channel counter
_afx_CHK1:
    djnz	_afx_CHK2

_afx_PLAY_C:
    call	_afx_SETMIXER
    ld	(#_g_PSG_Regs+10),A		; R#10 channel C volume
    bit	2,c						; tone off?
    jr	nz,_afx_END_decode
    ld	(#_g_PSG_Regs+4),HL		; R#4/5 channel C tone
    pop  IX
    ret

_afx_CHK2:
    rrc	d
    rrca
    djnz	_afx_CHK3

_afx_PLAY_B:
    call	_afx_SETMIXER
    ld	(#_g_PSG_Regs+9),A		; R#9 channel B volume
    bit	1,c
    jr	nz,_afx_END_decode
    ld	(#_g_PSG_Regs+2),HL		; R#2/3 channel B tone
    pop  IX
    ret

_afx_CHK3:
    rrc	d
    rrca

_afx_PLAY_A:
    call	_afx_SETMIXER
    ld	(#_g_PSG_Regs+8),A		; R#8 channel A volume
    bit	0,c
    jr	nz,_afx_END_decode
    ld	(#_g_PSG_Regs+0),HL		; R#0/1 channel A tone

_afx_STREAM_END:                ; end of stream: mark "nothing playing"
    ld	a,#255
    ld	(#_ayFX_PRIORITY),A

_afx_END_decode:
    pop  IX
    ret

_afx_SETMIXER:                  ; set this channel's mixer bits, keep the others
    ld	c,a						; c := original active-LOW OR mask.  IMPORTANT: c must
    ;                             stay this value -- _PLAY_A/B/C's "bit n,c" tone-write
    ;                             gate tests it (bit set = that channel's tone is off).
    ; --- msxmvl: convert to msxmvl's active-HIGH mixer convention (in scratch b) ---
    ;   enabled-bit set instead of clear. ~d = this channel's two bits; XOR with the
    ;   active-low OR mask (which marks the DISABLED bits) yields the ENABLED bits.
    ;   b is free here (the channel-dispatch counter is spent), so use it as the temp.
    ld	a,d
    cpl
    and	#0x3F
    xor	c
    ld	b,a						; b := active-high OR mask for this channel
    ; ---
    ld	A,(#_g_PSG_Regs+7)		; shared mixer
    and	d						; clear this channel's two bits
    or	b						; set the enabled ones
    ld	(#_g_PSG_Regs+7),A
    ld	A,(#_ayFX_VOLUME)
    ld	HL,(#_ayFX_TONE)
    ret
__endasm;
}
