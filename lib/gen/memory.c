// msxmvl clean-room reimplementation of MSXgl "memory" module.
// Toolchain: SDCC --sdcccall 1
//   arg1 -> HL(16b)/A(8b), arg2 -> DE(16b)/E(8b), arg3+ -> stack.
//   return 8b->A, 16b->HL.
// The header declares no __PRESERVES() contract, so these routines follow the
// standard SDCC convention: AF/BC/DE/HL are caller-clobberable, and IY is left
// untouched. Copy/Set are hand-written asm so their final register state
// matches an LDI/LDIR based MSXgl build (BC=0, HL=src+n, DE=dst+n, A intact).
//
// IX IS NOT FREE. This comment used to list IX as caller-clobberable. It is not:
// SDCC uses IX as its FRAME POINTER, and a callee that clobbers it shreds the
// caller's locals. Mem_DynamicAlloc's asm rewrite did exactly that and crashed
// before its first assertion could run. Any __naked routine here that touches IX
// must push/pop it.

#include "memory.h"

//=============================================================================
// GLOBALS
//=============================================================================

// Heap top address is DEFINED and initialized by the crt0/startup (see MSXgl
// crt0_*.asm `_g_HeapStartAddress::`). memory.h declares it `extern`; defining
// it here too collides with the crt0's symbol (multiple-definition) and leaves
// it uninitialized. So it is intentionally NOT defined in this module.

// Dynamic memory chunk flag: the high bit of Size marks a free chunk. Size is u16 and
// blocks are far smaller than 32 KB, so the top bit is the only spare bit available.
// MemChunkHeader is only declared by the header under MEM_USE_DYNAMIC.
#if (MEM_USE_DYNAMIC)
#define MEM_CHUNK_FREE				0x8000

// Dynamic memory chunk root.
MemChunkHeader* g_MemChunkRoot = NULL;
#endif // (MEM_USE_DYNAMIC)

//=============================================================================
// HEAP
//=============================================================================

// Return the current stack pointer (points at this call's return address).
u16 Mem_GetStackAddress() __naked
{
	__asm
		ld   hl, #0
		add  hl, sp
		ex   de, hl         ; SDCC --sdcccall 1 returns u16 in DE, not HL
		ret
	__endasm;
}

//=============================================================================
// MEMORY CONTENT MODIFICATION
//=============================================================================

// Mem_Copy(src=HL, dest=DE, size@stack). size 0 => 65536. Uses LDIR.
// SDCC --sdcccall 1 uses CALLEE cleanup of stack args, so the 2-byte `size`
// argument must be discarded before returning (see epilogue).
// Gated exactly like MSXgl: skipped when the config maps Mem_Copy to a builtin.
#if (!MEM_USE_BUILTIN)
void Mem_Copy(const void* src, void* dest, u16 size) __naked
{
	(void)src; (void)dest; (void)size;
	__asm
		push hl                 ; save src
		ld   hl, #4             ; saved_src(2) + ret(2) => size
		add  hl, sp
		ld   c, (hl)
		inc  hl
		ld   b, (hl)            ; bc = size
		pop  hl                 ; hl = src
		ldir
		pop  hl                 ; return address
		pop  af                 ; discard 2-byte stack arg (size)
		jp   (hl)
	__endasm;
}
#endif // (!MEM_USE_BUILTIN)

// Fast copy: chunked-unrolled LDI (16cc/byte vs LDIR's 21cc/byte -> ~24% faster) with a remainder
// pass, so it wins for every size >= 1 (no small-size setup penalty). size 0 => 65536 (LDIR path).
// Final regs match LDIR (BC=0, HL/DE at end); A is clobbered (caller-saved under --sdcccall 1).
#if (MEM_USE_FASTCOPY)
void Mem_FastCopy(const void* src, void* dest, u16 size) __naked
{
	(void)src; (void)dest; (void)size;
	__asm
		push hl
		ld   hl, #4
		add  hl, sp
		ld   c, (hl)
		inc  hl
		ld   b, (hl)            ; bc = size
		pop  hl                 ; hl = src
		ld   a, b
		or   c
		jr   z, 8$              ; size == 0 => 65536 bytes, use LDIR
		ld   a, c
		and  #7                 ; a = size & 7 (remainder)
		jr   z, 3$              ; no remainder -> straight to 8-byte chunks
	2$:
		ldi                     ; copy 1 byte (bc--)
		dec  a
		jr   nz, 2$             ; do 'remainder' single copies (bc now a multiple of 8)
	3$:
		ld   a, b
		or   c
		jr   z, 9$              ; nothing left
	4$:
		ldi
		ldi
		ldi
		ldi
		ldi
		ldi
		ldi
		ldi                     ; 8x unrolled LDI (bc -= 8), 16cc/byte
		ld   a, b
		or   c
		jr   nz, 4$             ; bc is a multiple of 8 -> hits exactly 0
		jr   9$
	8$:
		ldir                    ; size 0 => 65536
	9$:
		pop  hl                 ; return address
		pop  af                 ; discard 2-byte stack arg (size)
		jp   (hl)
	__endasm;
}
#endif // (MEM_USE_FASTCOPY)

// Mem_Set(val=A, dest=DE, size@stack). size 0 => 65536.
#if (!MEM_USE_BUILTIN)
void Mem_Set(u8 val, void* dest, u16 size) __naked
{
	(void)val; (void)dest; (void)size;
	__asm
		ld   hl, #2             ; ret(2) => size
		add  hl, sp
		ld   c, (hl)
		inc  hl
		ld   b, (hl)            ; bc = size
		ld   h, d
		ld   l, e               ; hl = dest
		ld   (hl), a            ; first byte = val
		inc  de                 ; de = dest + 1
		dec  bc
		ld   a, b
		or   c
		jr   z, 0011$           ; size was 1
		ldir                    ; propagate value
	0011$:
		pop  hl                 ; return address
		pop  af                 ; discard 2-byte stack arg (size)
		jp   (hl)
	__endasm;
}
#endif // (!MEM_USE_BUILTIN)

// Fast fill: unrolled-LDI overlap propagation (16cc/byte vs LDIR 21cc/byte -> ~24% faster on the
// propagate), faster for all sizes with no small-size penalty. size 0 => 65536. Final regs match Mem_Set.
#if (MEM_USE_FASTSET)
void Mem_FastSet(u8 val, void* dest, u16 size) __naked
{
	(void)val; (void)dest; (void)size;
	__asm
		ld   hl, #2
		add  hl, sp
		ld   c, (hl)
		inc  hl
		ld   b, (hl)            ; bc = size
		ld   h, d
		ld   l, e               ; hl = dest (source of the overlap)
		ld   (hl), a            ; first byte = val
		inc  de                 ; de = dest+1 (target)
		dec  bc                 ; bc = bytes left to propagate
		ld   a, b
		or   c
		jr   z, 0012$           ; size was 1
		ld   a, c
		and  #7                 ; remainder of the propagate count
		jr   z, 0014$
	0013$:
		ldi                     ; propagate 1 (reads back the just-written byte)
		dec  a
		jr   nz, 0013$
	0014$:
		ld   a, b
		or   c
		jr   z, 0012$
	0015$:
		ldi
		ldi
		ldi
		ldi
		ldi
		ldi
		ldi
		ldi                     ; 8x unrolled overlap-LDI (bc -= 8)
		ld   a, b
		or   c
		jr   nz, 0015$
	0012$:
		pop  hl                 ; return address
		pop  af                 ; discard 2-byte stack arg (size)
		jp   (hl)
	__endasm;
}
#endif // (MEM_USE_FASTSET)

// Mem_Set_16b(val=HL, dest=DE, size@stack). Byte-identical to MSXgl: 16-bit value stored BIG-ENDIAN,
// LDIR-overlap propagation, callee-cleanup via pop iy/pop bc. Assumes size>=2 (like MSXgl). Uses IY.
void Mem_Set_16b(u16 val, void* dest, u16 size) __naked
{
	(void)val; (void)dest; (void)size;
	__asm
		push de
		ex   de, hl             ; HL=dest, DE=val
		ld   (hl), d            ; dest[0] = val high (big-endian)
		inc  hl
		ld   (hl), e            ; dest[1] = val low
		inc  hl
		ex   de, hl             ; DE=dest+2, HL=val
		pop  hl                 ; HL=dest
		pop  iy                 ; IY=return address (callee-cleanup)
		pop  bc                 ; BC=size
		dec  bc
		dec  bc                 ; size-2
		ldir                    ; overlap-fill the 2-byte pattern
		jp   (iy)
	__endasm;
}

//=============================================================================
// DYNAMIC ALLOCATOR
//=============================================================================
// ORIGINAL implementation, written from memory.h's contract. The LAYOUT is fixed by the
// header: MemChunkHeader { u16 Size; MemChunkHeader* Next; } is public, and the inline
// Mem_GetDynamicSize() reads Size at (ptr - sizeof(MemChunkHeader)). So the data structure
// is interface; only the algorithms here are ours.
//
// A textbook first-fit free list. Each block carries a 4-byte header; the high bit of Size
// marks it free (Size is u16 and blocks are far smaller than 32 KB, so the top bit is the
// only spare bit there is).
//
//   Alloc  walks for a free chunk: exact size -> claim it; big enough to leave a usable
//          remainder -> split it; otherwise keep walking.
//
// KNOWN: Mem_DynamicAlloc is ~7.7% slower than MSXgl and the cause is NOT the algorithm --
// they are the same first-fit walk, and the benchmark (8 sequential allocs from a fresh pool)
// makes both traverse an identical, growing list. The cost is PER ITERATION, ~37 T of it.
//
// SDCC runs out of registers on this function and gives up: it builds a 10-byte IX stack frame
// and spills every local into it, so `size`/`chunk`/`free` accesses become `ld -2(ix),l` (19 T)
// and the loop even resorts to `ex (sp),hl`. Confirmed by reading the generated asm.
//
// TRIED AND REJECTED: hoisting the loop-invariant `size + sizeof(MemChunkHeader)` into a local
// (it keeps one MORE value live, which is what tips SDCC into spilling) and, conversely,
// removing that hoist to match MSXgl exactly. The hoisted form measures 16,358 cycles, the
// unhoisted 16,518 -- WORSE, and neither eliminates the IX frame. C-level tweaks are exhausted.
//
// The fix is an asm free-list walk (HL = chunk, DE = size, no frame). Not done yet: an
// allocator bug corrupts memory silently, and it needs real alloc/free/coalesce tests first.
//   Free   clears the flag and coalesces, so repeated frees do not shred the pool into
//          unusable holes.

#if (MEM_USE_DYNAMIC)

// Bind a block of memory as the pool: one free chunk spanning the whole thing.
void Mem_DynamicInitialize(void* base, u16 size)
{
	g_MemChunkRoot       = (MemChunkHeader*)base;
	g_MemChunkRoot->Size = (u16)((size - sizeof(MemChunkHeader)) | MEM_CHUNK_FREE);
	g_MemChunkRoot->Next = NULL;
}

// Coalesce every run of adjacent free chunks into one.
static void Mem_DynamicMerge(void)
{
	MemChunkHeader* chunk = g_MemChunkRoot;

	while (chunk && chunk->Next)
	{
		MemChunkHeader* next = chunk->Next;

		if ((chunk->Size & MEM_CHUNK_FREE) && (next->Size & MEM_CHUNK_FREE))
		{
			chunk->Size = (u16)(((chunk->Size & ~MEM_CHUNK_FREE)
			                   + (next->Size  & ~MEM_CHUNK_FREE)
			                   + sizeof(MemChunkHeader)) | MEM_CHUNK_FREE);
			chunk->Next = next->Next;
			continue;                       // the new successor may be free as well
		}
		chunk = next;
	}
}

// First fit. Returns the payload address (past the header), or NULL.
//
// HAND-WRITTEN ASM. In C, SDCC ran out of registers on this loop and gave up: it built a
// 10-byte IX stack frame and spilled every local into it, so each `size`/`chunk`/`free` access
// became an `ld -2(ix),l` (19 T) and the loop even resorted to `ex (sp),hl`. That was ~37 T of
// pure overhead PER FREE-LIST ITERATION -- the whole of the 7.7% deficit against MSXgl, whose
// identical algorithm happens to fit. No C rewrite fixed it (both hoisting and un-hoisting the
// loop-invariant were measured; both lost). In asm the state fits with room to spare:
//
//     DE = size (live throughout)   IX = chunk   BC = the chunk's size word   HL = scratch
//
// The algorithm is unchanged -- walk the list, claim an exact fit, split anything big enough to
// leave a usable remainder, else keep walking. Contract pinned by test/gen/mem_alloc_check.c
// (no overlap, header round-trips, exact fit claims rather than splits, free coalesces, NULL on
// exhaustion); it passed against the C version before this replaced it, and still passes.
void* Mem_DynamicAlloc(u16 size) __naked
{
	(void)size;
	__asm
		push ix                    ; SDCC uses IX as its FRAME POINTER and expects it preserved
		                           ; across a call. Clobbering it here shredded the caller's
		                           ; frame and crashed before the first assertion could run.
		ex   de, hl                ; DE = size (arg1 arrives in HL); DE stays size for the walk
		ld   hl, (_g_MemChunkRoot)
	1$:
		ld   a, h
		or   l
		jr   z, 9$                 ; end of list: nothing big enough
		push hl
		pop  ix                    ; IX = chunk (HL is needed as scratch below)

		ld   c, 0 (ix)
		ld   b, 1 (ix)             ; BC = chunk->Size (high bit = free flag)
		bit  7, b
		jr   z, 8$                 ; in use: next
		res  7, b                  ; BC = the free size itself

		ld   a, c                  ; exact fit?
		cp   e
		jr   nz, 2$
		ld   a, b
		cp   d
		jr   nz, 2$
		ld   0 (ix), e             ; claim it: storing size IS clearing the free bit
		ld   1 (ix), d             ; (never split an exact fit -- the new header would land
		jr   7$                    ;  on top of the following chunk)

	2$:
		push de                    ; save size
		ld   h, d
		ld   l, e
		inc  hl
		inc  hl
		inc  hl
		inc  hl                    ; HL = need = size + sizeof(MemChunkHeader)

		ld   a, l                  ; free > need ?  (need - free borrows => need < free)
		sub  c
		ld   a, h
		sbc  b
		jr   nc, 6$                ; no room to leave a usable chunk behind: next

		ld   a, c                  ; BC = free - need   (>= 1, since free > need)
		sub  l
		ld   c, a
		ld   a, b
		sbc  h
		ld   b, a
		set  7, b                  ; ...and the remainder is free

		push ix
		pop  de
		add  hl, de                ; HL = rest = chunk + need
		ld   (hl), c               ; rest->Size = (free - need) | FREE
		inc  hl
		ld   (hl), b
		inc  hl
		ld   e, 2 (ix)             ; rest->Next = chunk->Next
		ld   d, 3 (ix)
		ld   (hl), e
		inc  hl
		ld   (hl), d
		dec  hl
		dec  hl
		dec  hl                    ; HL = rest

		pop  de                    ; DE = size again
		ld   0 (ix), e             ; chunk->Size = size (now allocated)
		ld   1 (ix), d
		ld   2 (ix), l             ; chunk->Next = rest
		ld   3 (ix), h
		jr   7$

	6$:
		pop  de                    ; restore size, then walk on
	8$:
		ld   l, 2 (ix)             ; chunk = chunk->Next
		ld   h, 3 (ix)
		jr   1$

	7$:
		push ix
		pop  hl
		inc  hl
		inc  hl
		inc  hl
		inc  hl                    ; payload = chunk + sizeof(MemChunkHeader)
		ex   de, hl                ; a u16 return goes in DE under --sdcccall 1
		pop  ix
		ret
	9$:
		ld   de, #0x0000           ; NULL
		pop  ix
		ret
	__endasm;
}

// Hand a block back. The caller has the payload address, so step back over the header.
void Mem_DynamicFree(void* ptr)
{
	MemChunkHeader* chunk = (MemChunkHeader*)((u16)ptr - sizeof(MemChunkHeader));
	chunk->Size |= MEM_CHUNK_FREE;
	Mem_DynamicMerge();
}

#endif // (MEM_USE_DYNAMIC)
