// msxmvl clean-room reimplementation of MSXgl "dos_mapper" module.
// Toolchain: SDCC --sdcccall 1
//   arg1 -> HL(16b)/A(8b), arg2 -> DE(16b)/E(8b), arg3+ -> stack (callee-cleanup;
//   8-bit stack args occupy 1 byte). return 8b->A, 16b->HL.
//
// MSX-DOS 2 installs a memory-mapper extension reachable through the extended
// BIOS hook EXTBIO (0xFFCA). Two "get" queries retrieve, respectively, the
// Memory Mapper variable table (a pointer) and the jump table of the mapper
// support routines (ALL_SEG, FRE_SEG, RD_SEG, ...). The jump table lives in the
// always-mapped system area (page 3) so its entries are callable directly.
//
// EXTBIO query convention: A = 0, D = 0x04 (memory-mapper device),
// E = sub-function (variable table = 2, jump table = 3).
//
// The header declares no __PRESERVES() contract, so these routines follow the
// standard SDCC convention: AF/BC/DE/HL are caller-clobberable, but IX (the
// SDCC frame pointer) and IY must be preserved. Every routine that loads IX to
// reach the mapper jump table therefore push/pop's it.

#include "dos_mapper.h"

//=============================================================================
// GLOBALS
//=============================================================================

// Pointer to the Memory Mapper variable table (filled by DOSMapper_Init).
DOS_VarTable* g_DOS_VarTable;

// Start address of the mapper support routines jump table.
u16 g_DOS_JumpTable;

//=============================================================================
// CORE
//=============================================================================

// DOSMapper_Init(): query the extended BIOS for the mapper variable table and
// jump table. Returns TRUE when the extended BIOS (hence a mapper) is present.
bool DOSMapper_Init() __naked
{
	__asm
		; Extended BIOS installed ? (HOKVLD bit 0 at 0xFB20)
		ld   a, (0xFB20)
		and  #0x01
		jr   nz, init_have_ext
		xor  a                  ; A = FALSE
		ret
	init_have_ext:
		push iy                 ; EXTBIO may clobber IY (SDCC frame pointer)
		; Get the Memory Mapper variable table -> HL
		xor  a
		ld   de, #0x0402
		call 0xFFCA
		ld   (_g_DOS_VarTable), hl
		; Get the mapper support routines jump table -> HL
		xor  a
		ld   de, #0x0403
		call 0xFFCA
		ld   (_g_DOS_JumpTable), hl
		pop  iy
		ld   a, #0x01           ; A = TRUE
		ret
	__endasm;
}

//=============================================================================
// ALLOCATION
//=============================================================================

// DOSMapper_Alloc(type=A, slot=E, seg@stack):
//   ALL_SEG in:  A = type (0 user / 1 sys), B = slot (FxxxSSPP)
//           out: A = segment number, B = slot, Cf = error
// Writes {Number, Slot} to *seg. Returns TRUE on success (no carry).
bool DOSMapper_Alloc(u8 type, u8 slot, DOS_Segment* seg) __naked
{
	(void)type; (void)slot; (void)seg;
	__asm
		; ALL_SEG is jump-table entry 0 (MSX-DOS 2 mapper support spec). Call it with A = type,
		; B = slot; it returns the segment in A, the slot in B, and error in the carry flag.
		; Both exit paths pop the 2-byte seg-pointer stack arg (pop iy = ret addr, pop hl = seg
		; ptr) before returning via IY -- a plain xor-a/ret error path would LEAK that stack arg
		; under SDCC --sdcccall 1 callee-cleanup and unbalance the caller's stack.
		push ix                 ; preserve the SDCC frame pointer across the mapper routine
		ld   b, l
		ld   hl, (_g_DOS_JumpTable)
		call ___sdcc_call_hl    ; call ALL_SEG (offset 0); A=seg, B=slot, Cf=error
		pop  ix                 ; restore the frame pointer (flags preserved)
		pop  iy                 ; return address (flags preserved)
		pop  hl                 ; segment struct address (callee-cleanup; flags preserved)
		jr   c, alloc_error
		ld   (hl), a            ; store segment number
		inc  hl
		ld   a, b
		ld   (hl), a            ; store slot number
		ld   a, #0xFF           ; return TRUE
		jp   (iy)
	alloc_error:
		xor  a                  ; return FALSE
		jp   (iy)
	__endasm;
}

// DOSMapper_Free(seg=A, slot=E):
//   FRE_SEG in: A = segment number, B = slot ; out: Cf = error
// Returns TRUE on success (no carry). FRE_SEG is jump-table entry 3 (MSX-DOS 2 mapper spec).
bool DOSMapper_Free(u8 seg, u8 slot) __naked
{
	(void)seg; (void)slot;
	__asm
		push ix                 ; backup the frame pointer
		ld   b, l
		ld   hl, (_g_DOS_JumpTable)
		ld   de, #0x03          ; DOS_FRE_SEG
		add  hl, de
		call ___sdcc_call_hl    ; call FRE_SEG
		ld   a, #0
		adc  #0xFF              ; Cf=0 -> A=0xFF (TRUE), Cf=1 -> A=0x00 (FALSE)
		pop  ix                 ; restore the frame pointer
		ret
	__endasm;
}

//=============================================================================
// PAGE I/O
//=============================================================================

// DOSMapper_ReadByte(seg=A, addr=DE):
//   RD_SEG in: A = segment, HL = address ; out: A = byte
u8 DOSMapper_ReadByte(u8 seg, u16 addr) __naked
{
	(void)seg; (void)addr;
	__asm
		ex   de, hl             ; address in HL (A = segment unchanged)
		ld   iy, (_g_DOS_JumpTable)
		ld   de, #0x06          ; DOS_RD_SEG
		add  iy, de
		call ___sdcc_call_iy    ; call RD_SEG -> A = byte read
		ei
		ret
	__endasm;
}

// DOSMapper_WriteByte(seg=A, addr=DE, val@stack):
//   WR_SEG in: A = segment, HL = address, E = value
void DOSMapper_WriteByte(u8 seg, u16 addr, u8 val) __naked
{
	(void)seg; (void)addr; (void)val;
	__asm
		; WR_SEG is jump-table entry 9 (MSX-DOS 2 mapper spec): A = segment, HL = address,
		; E = value. The epilogue pops the return address and drops the 1-byte val stack arg --
		; a bare `ret` would leak it under SDCC --sdcccall 1 callee-cleanup.
		ex   de, hl             ; address in HL
		ld   iy, #2
		add  iy, sp
		ld   e, 0(iy)           ; E = val
		ld   iy, (_g_DOS_JumpTable)
		ld   bc, #0x09          ; DOS_WR_SEG
		add  iy, bc
		call ___sdcc_call_iy    ; call WR_SEG
		ei
		pop  hl                 ; return address
		inc  sp                 ; drop 1-byte val stack arg (callee-cleanup)
		jp   (hl)
	__endasm;
}

//=============================================================================
// PAGE HANDLING
//=============================================================================

// DOSMapper_SetPage(page=A, seg=L):
//   PUT_PH in: A = segment, H = address hi (page = bits 14-15). Jump-table entry 0x12 (MSX-DOS 2
//   mapper spec); jp (iy) tail-calls it so PUT_PH's own ret returns to our caller.
void DOSMapper_SetPage(u8 page, u8 seg) __naked
{
	(void)page; (void)seg;
	__asm
		rrca
		rrca
		ld   h, a
		ld   a, l
		ld   iy, (_g_DOS_JumpTable)
		ld   de, #0x12          ; DOS_PUT_PH
		add  iy, de
		jp   (iy)               ; call PUT_PH
	__endasm;
}

// DOSMapper_GetPage(page=A):
//   GET_PH in: H = address hi (page = bits 14-15) ; out: A = segment. Jump-table entry 0x15;
//   tail-called, so GET_PH's ret carries A back to our caller.
u8 DOSMapper_GetPage(u8 page) __naked
{
	(void)page;
	__asm
		rrca
		rrca
		ld   h, a
		ld   iy, (_g_DOS_JumpTable)
		ld   de, #0x15          ; DOS_GET_PH
		add  iy, de
		jp   (iy)               ; call GET_PH | return in A
	__endasm;
}

// DOSMapper_SetPage0(seg=A): PUT_P0 in A = segment. Jump-table entry 0x18 (MSX-DOS 2 spec), tail-called.
void DOSMapper_SetPage0(u8 seg) __naked
{
	(void)seg;
	__asm
		ld   hl, (_g_DOS_JumpTable)
		ld   de, #0x18          ; DOS_PUT_P0
		add  hl, de
		jp   (hl)               ; call PUT_P0
	__endasm;
}

// DOSMapper_SetPage1(seg=A): PUT_P1 in A = segment. Jump-table entry 0x1E (MSX-DOS 2 spec), tail-called.
void DOSMapper_SetPage1(u8 seg) __naked
{
	(void)seg;
	__asm
		ld   hl, (_g_DOS_JumpTable)
		ld   de, #0x1E          ; DOS_PUT_P1
		add  hl, de
		jp   (hl)               ; call PUT_P1
	__endasm;
}

// DOSMapper_SetPage2(seg=A): PUT_P2 in A = segment. Jump-table entry 0x24 (MSX-DOS 2 spec), tail-called.
void DOSMapper_SetPage2(u8 seg) __naked
{
	(void)seg;
	__asm
		ld   hl, (_g_DOS_JumpTable)
		ld   de, #0x24          ; DOS_PUT_P2
		add  hl, de
		jp   (hl)               ; call PUT_P2
	__endasm;
}

// DOSMapper_GetPage0(): GET_P0 out A = segment. Jump-table entry 0x1B (MSX-DOS 2 spec), tail-called.
u8 DOSMapper_GetPage0() __naked
{
	__asm
		ld   hl, (_g_DOS_JumpTable)
		ld   de, #0x1B          ; DOS_GET_P0
		add  hl, de
		jp   (hl)               ; call GET_P0 | return in A
	__endasm;
}

// DOSMapper_GetPage1(): GET_P1 out A = segment. Jump-table entry 0x21 (MSX-DOS 2 spec), tail-called.
u8 DOSMapper_GetPage1() __naked
{
	__asm
		ld   hl, (_g_DOS_JumpTable)
		ld   de, #0x21          ; DOS_GET_P1
		add  hl, de
		jp   (hl)               ; call GET_P1 | return in A
	__endasm;
}

// DOSMapper_GetPage2(): GET_P2 out A = segment. Jump-table entry 0x27 (MSX-DOS 2 spec), tail-called.
u8 DOSMapper_GetPage2() __naked
{
	__asm
		ld   hl, (_g_DOS_JumpTable)
		ld   de, #0x27          ; DOS_GET_P2
		add  hl, de
		jp   (hl)               ; call GET_P2 | return in A
	__endasm;
}

// DOSMapper_GetPage3(): GET_P3 out A = segment. Jump-table entry 0x2D (MSX-DOS 2 spec), tail-called.
u8 DOSMapper_GetPage3() __naked
{
	__asm
		ld   hl, (_g_DOS_JumpTable)
		ld   de, #0x2D          ; DOS_GET_P3
		add  hl, de
		jp   (hl)               ; call GET_P3 | return in A
	__endasm;
}
