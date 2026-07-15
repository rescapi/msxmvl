// msxmvl clean-room reimplementation of MSXgl "bios" module (out-of-line part).
// Toolchain: SDCC --sdcccall 1
//   arg1 -> HL(16b)/A(8b), arg2 -> DE(16b)/E(8b), arg3+ -> stack (callee-clean).
//   return 8b->A, 16b->HL.
// The header declares no __PRESERVES() contract, so these follow the standard
// SDCC convention. IY (SDCC frame pointer) is preserved across the slot
// routines that may clobber it (CALSLT). BIOS routine register inputs are set
// up by hand where they differ from the SDCC parameter registers.

#include "bios.h"

//=============================================================================
// Group: Helper
//=============================================================================

// Handle clean exit from Basic or MSX-DOS environment.
// DOS2: _TERM (function 62h), return code in B. DOS1 fallback: system reset
// (function 00h). Under a bare-BIOS/BASIC target this simply returns to BDOS.
void BIOS_Exit(u8 ret) __naked
{
	(void)ret;
	__asm
		ld   b, a          ; B = return code (DOS2 _TERM)
		ld   c, #0x62
		call 0x0005        ; BDOS _TERM (DOS2) - does not return under DOS
		ld   c, #0x00      ; DOS1 fallback: system reset / return to command
		call 0x0005
		ret
	__endasm;
}

//=============================================================================
// Group: Main-ROM - Core
//=============================================================================

// Reads the value at 'addr' in 'slot'. Wrapper for RDSLT (A=slot, HL=addr -> A).
u8 BIOS_InterSlotRead(u8 slot, u16 addr) __naked
{
	(void)slot; (void)addr;
	__asm
		ex   de, hl        ; HL = addr (A = slot)
		call 0x000C        ; RDSLT -> A = value (does its own DI)
		ei
		ret                ; return value in A
	__endasm;
}

// Writes 'value' at 'addr' in 'slot'. Wrapper for WRSLT (A=slot, HL=addr, E=value).
void BIOS_InterSlotWrite(u8 slot, u16 addr, u8 value) __naked
{
	(void)slot; (void)addr; (void)value;
	__asm
		push iy            ; preserve caller IY
		ld   c, a          ; C = slot (free up A to read the stack)
		ld   hl, #4        ; saved iy(2) + ret(2) => value at SP+4
		add  hl, sp
		ld   b, (hl)       ; B = value
		ex   de, hl        ; HL = addr
		ld   e, b          ; E = value
		ld   a, c          ; A = slot
		di
		call 0x0014        ; WRSLT (A=slot, HL=addr, E=value)
		ei
		pop  iy            ; restore caller IY
		pop  hl            ; HL = return address
		inc  sp            ; discard 1-byte stack arg (value)
		jp   (hl)
	__endasm;
}

// Executes inter-slot call. Wrapper for CALSLT (IYH=slot, IX=addr).
void BIOS_InterSlotCall(u8 slot, u16 addr) __naked
{
	(void)slot; (void)addr;
	__asm
		push iy
		ex   de, hl        ; HL = addr
		push hl
		pop  ix            ; IX = addr
		ld   h, a          ; H = slot
		ld   l, #0
		push hl
		pop  iy            ; IYH = slot
		call 0x001C        ; CALSLT
		pop  iy
		ret
	__endasm;
}

// Switches to specified slot in the page containing 'page'. Wrapper for ENASLT
// (A=slot ID, H bits 6-7 select page). page in A, slot in E.
void BIOS_SwitchSlot(u8 page, u8 slot) __naked
{
	(void)page; (void)slot;
	// NOTE: MSXgl's BIOS_SwitchSlot reads the slot from H, but under --sdcccall 1
	// the 2nd u8 arg is passed in L. We read L (correct); otherwise ENASLT would
	// switch to a garbage slot. Same size (12B) as MSXgl, but correct.
	__asm
		; A = page, L = slot (SDCC (u8,u8) -> A, L)
		rrca               ; page (0-3) -> bits 7-6
		rrca
		and  a, #0xC0
		ld   b, l          ; B = slot ID (arg2 in L)
		ld   h, a          ; H = page << 6
		ld   a, b          ; A = slot ID
		call 0x0024        ; ENASLT (does its own DI)
		ei                 ; ENASLT leaves interrupts disabled
		ret
	__endasm;
}

//=============================================================================
// Group: Main-ROM - VDP
//=============================================================================

// Writes 'value' to VDP register 'reg'. Wrapper for WRTVDP (B=data, C=reg).
void BIOS_WriteVDP(u8 reg, u8 value) __naked
{
	(void)reg; (void)value;
	__asm
		; A = reg, L = value (SDCC (u8,u8) -> A, L)
		ld   c, a          ; C = register number
		ld   b, l          ; B = data
		call 0x0047        ; WRTVDP
		ret
	__endasm;
}

// Writes 'value' at VRAM 'addr'. Wrapper for WRTVRM (A=data, HL=addr).
void BIOS_WriteVRAM(u8 value, u16 addr) __naked
{
	(void)value; (void)addr;
	__asm
		ex   de, hl        ; HL = addr (A = value)
		call 0x004D        ; WRTVRM
		ret
	__endasm;
}

// Fills 'length' bytes of VRAM from 'addr' with 'value'.
// Wrapper for FILVRM (HL=addr, BC=length, A=data). length occupies 2 stack bytes.
void BIOS_FillVRAM(u16 addr, u16 length, u8 value) __naked
{
	(void)addr; (void)length; (void)value;
	__asm
		; HL = addr, DE = length, value (1 byte) at SP+2
		ld   b, d
		ld   c, e          ; BC = length
		push hl            ; save addr (value now at SP+4)
		ld   hl, #4
		add  hl, sp
		ld   a, (hl)       ; A = value
		pop  hl            ; HL = addr
		call 0x0056        ; FILVRM (HL=addr, BC=length, A=value)
		pop  hl            ; HL = return address
		inc  sp            ; discard 1-byte stack arg (value)
		jp   (hl)
	__endasm;
}

// Copies 'length' bytes from VRAM 'vram' to RAM 'ram'.
// Wrapper for LDIRMV (HL=src VRAM, DE=dst RAM, BC=length). length = 2 stack bytes.
void BIOS_CopyFromVRAM(u16 vram, void* ram, u16 length) __naked
{
	(void)vram; (void)ram; (void)length;
	__asm
		; HL = vram (src), DE = ram (dst), length at SP+2
		push hl
		push de
		ld   hl, #6        ; saved hl(2) + saved de(2) + ret(2)
		add  hl, sp
		ld   c, (hl)
		inc  hl
		ld   b, (hl)       ; BC = length
		pop  de
		pop  hl
		call 0x0059        ; LDIRMV
		pop  hl            ; HL = return address
		pop  de            ; discard 2-byte stack arg (length)
		jp   (hl)
	__endasm;
}

// Copies 'length' bytes from RAM 'ram' to VRAM 'vram'.
// Wrapper for LDIRVM (HL=src RAM, DE=dst VRAM, BC=length). length = 2 stack bytes.
void BIOS_CopyToVRAM(const void* ram, u16 vram, u16 length) __naked
{
	(void)ram; (void)vram; (void)length;
	__asm
		; HL = ram (src), DE = vram (dst), length at SP+2
		push hl
		push de
		ld   hl, #6
		add  hl, sp
		ld   c, (hl)
		inc  hl
		ld   b, (hl)       ; BC = length
		pop  de
		pop  hl
		call 0x005C        ; LDIRVM
		pop  hl            ; HL = return address
		pop  de            ; discard 2-byte stack arg (length)
		jp   (hl)
	__endasm;
}

// Switches to SCREEN 3 with explicit table setup, then INIMLT.
void BIOS_InitScreen3Ex(u16 pnt, u16 ct, u16 pgt, u16 sat, u16 sgt, u8 text, u8 bg, u8 border)
{
	g_MLTNAM = pnt;
	g_MLTCOL = ct;
	g_MLTCGP = pgt;
	g_MLTATR = sat;
	g_MLTPAT = sgt;
	g_FORCLR = text;
	g_BAKCLR = bg;
	g_BDRCLR = border;
	Call(R_INIMLT);
}

// Returns VRAM address of sprite pattern 'id'. Wrapper for CALPAT (A=id -> HL).
// The ROM routine delivers the computed address in HL; SDCC --sdcccall 1 expects
// a u16 return value in DE, so exchange HL<->DE before returning.
u16 BIOS_GetSpritePatternAddress(u8 id) __naked
{
	(void)id;
	__asm
		call 0x0084        ; CALPAT (A=id) -> HL = address
		ex   de, hl        ; DE = address = return value
		ret
	__endasm;
}

// Returns VRAM address of sprite attribute 'id'. Wrapper for CALATR (A=id -> HL).
u16 BIOS_GetSpriteAttributeAddress(u8 id) __naked
{
	(void)id;
	__asm
		call 0x0087        ; CALATR (A=id) -> HL = address
		ex   de, hl        ; DE = address = return value
		ret
	__endasm;
}

//=============================================================================
// Group: Main-ROM - PSG
//=============================================================================

// Writes 'value' to PSG register 'reg'. Wrapper for WRTPSG (A=reg, E=data).
void BIOS_WritePSG(u8 reg, u8 value) __naked
{
	(void)reg; (void)value;
	__asm
		; A = reg, L = value (SDCC (u8,u8) -> A, L)
		ld   e, l          ; E = data
		call 0x0093        ; WRTPSG (A=reg, E=data)
		ret
	__endasm;
}

//=============================================================================
// Group: Main-ROM - Console
//=============================================================================

// Get a character input (if any) or return 0. Uses CHSNS to test, then CHGET.
u8 BIOS_HasCharacter() __naked
{
	__asm
		ld   l, #0
		call 0x009C        ; CHSNS: Z set if keyboard buffer empty (A=0)
		ret  z             ; Return 0
		call 0x009F        ; CHGET: returns available char in A (no wait)
		ret
	__endasm;
}

// Moves cursor to (x, y). Wrapper for POSIT (H=x column, L=y row).
void BIOS_TextSetCursor(u8 x, u8 y) __naked
{
	(void)x; (void)y;
	__asm
		; A = x, L = y (SDCC (u8,u8) -> A, L); POSIT wants H = x, L = y
		ld   h, a          ; H = x (column)
		call 0x00C6        ; POSIT (L already = y)
		ret
	__endasm;
}

// Clears the screen. Wrapper for CLS (requires ZF set / A = 0).
void BIOS_ClearScreen() __naked
{
	__asm
		xor  a             ; A = 0, ZF set (CLS entry condition)
		call 0x00C3        ; CLS
		ret
	__endasm;
}
