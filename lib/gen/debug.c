// msxmvl — clean-room reimplementation of MSXgl "debug" module.
//
// Target tool: openMSX "PVM" debugger script (DEBUG_OPENMSX_P). Protocol as
// documented in lib/gen/debug.h and MSXgl tools/script/openMSX/
// debugger_pvm.tcl:
//   * control port 0x2E : 0xFF => break, other => debug_mode
//   * data    port 0x2F : two writes (low, high) deliver the 16-bit address
//                         of an in-memory argument block
//                             [format-ptr:2][arg0][arg1]...
//
// All routines are __naked so the argument registers / on-stack parameter
// layout produced by SDCC (--sdcccall 1) are used exactly as the tool expects.
// None of them touch IX or IY, so the frame pointer and index registers are
// preserved across every call.

#include "debug.h"

// When the build disables debugging, MSXgl's debug.h turns DEBUG_INIT/LOG/... into
// empty macros (so `void DEBUG_INIT() __naked {}` would expand to garbage). Emit the
// real bodies only when DEBUG_INIT is still a function (not a macro) — true for the
// standalone msxmvl build and for MSXgl builds with debugging enabled.
#ifndef DEBUG_INIT

//=============================================================================
// FORMAT STRINGS (read out of memory by the debugger)
//=============================================================================
// Kept non-static so their asm labels (_g_DebugFmtLog / _g_DebugFmtLogNum)
// are referenceable from the inline assembly below.

const c8 g_DebugFmtLog[]    = "%s\n";     // DEBUG_LOG   : string + newline
const c8 g_DebugFmtLogNum[] = "%s%hhu\n"; // DEBUG_LOGNUM: string + u8 + newline

//=============================================================================
// GROUP: DEBUG
//=============================================================================

//-----------------------------------------------------------------------------
// Initialize Debug module. Nothing to reserve on openMSX; pure no-op.
void DEBUG_INIT() __naked
{
	__asm
		ret
	__endasm;
}

//-----------------------------------------------------------------------------
// Force a break point: request a break via the control port.
void DEBUG_BREAK() __naked
{
	__asm
		ld	a, #0xFF		; DEBUG_BREAK_CODE
		out	(0x2E), a		; P_DEBUG_CTRL
		ret
	__endasm;
}

//-----------------------------------------------------------------------------
// Conditional break: a == 0 (false) triggers a break, otherwise return.
void DEBUG_ASSERT(bool a) __naked
{
	a;	// A = a
	__asm
		or	a, a			; test the argument (in A)
		ret	NZ			; non-zero => condition true, no break
		ld	a, #0xFF		; false => request break
		out	(0x2E), a
		ret
	__endasm;
}

//-----------------------------------------------------------------------------
// Display a message followed by a new line.
// Builds the block [ &"%s\n" , msg ] on the stack and hands its address to
// the tool via two writes to the data port.
void DEBUG_LOG(const c8* msg) __naked
{
	msg;	// HL = msg
	__asm
		push	hl			; block+2 : msg (argument for %s)
		ld	hl, #_g_DebugFmtLog
		push	hl			; block+0 : &format
		ld	hl, #0
		add	hl, sp			; HL = block address
		ld	a, l
		out	(0x2F), a		; data port: low byte
		ld	a, h
		out	(0x2F), a		; data port: high byte -> tool reads block
		pop	hl			; drop &format
		pop	hl			; drop msg
		ret
	__endasm;
}

//-----------------------------------------------------------------------------
// Display a message and an 8-bit value, followed by a new line.
// SDCC passes the 8-bit second argument on the stack (1 byte at SP+2);
// msg arrives in HL. Builds [ &"%s%hhu\n" , msg , num-as-word ] on the stack.
void DEBUG_LOGNUM(const c8* msg, u8 num) __naked
{
	msg; num;	// HL = msg, num @ SP+2 (1 byte)
	__asm
		ld	c, l
		ld	b, h			; BC = msg (free up HL)
		ld	hl, #2
		add	hl, sp			; HL -> num on the stack
		ld	l, (hl)			; L = num
		ld	h, #0			; HL = num promoted to 16-bit
		push	hl			; block+4 : num word (only low byte read by %hhu)
		push	bc			; block+2 : msg
		ld	hl, #_g_DebugFmtLogNum
		push	hl			; block+0 : &format
		ld	hl, #0
		add	hl, sp			; HL = block address
		ld	a, l
		out	(0x2F), a		; data port: low byte
		ld	a, h
		out	(0x2F), a		; data port: high byte -> tool reads block
		pop	hl			; drop &format
		pop	hl			; drop msg
		pop	hl			; drop num word
		ret
	__endasm;
}

//-----------------------------------------------------------------------------
// Display a formatted message (no trailing new line).
// SDCC lays out a variadic call's parameters contiguously on the stack, so
// the parameter list starting at &format IS the argument block the tool wants:
//     [format-ptr][arg0][arg1]...
// We simply send &format (= SP+2 at entry).
void DEBUG_PRINT(const c8* format, ...) __naked
{
	format;
	__asm
		ld	hl, #2
		add	hl, sp			; HL = &format = argument block address
		ld	a, l
		out	(0x2F), a		; data port: low byte
		ld	a, h
		out	(0x2F), a		; data port: high byte -> tool reads block
		ret
	__endasm;
}

#endif // !defined(DEBUG_INIT)
