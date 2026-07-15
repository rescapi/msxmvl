// msxmvl EXTENSION module: farmem -- opaque far pointers over memseg.
// See farmem.h for the design rationale.

#include "farmem.h"

// Real address in the window page for a given in-segment offset.
#define WIN(ofs)  ((volatile u8*)(MEMSEG_WINDOW + (ofs)))

void Far_With_Fast(FarPtr p, u16 len, FarBorrowFn fn, void* ctx)
{
	MemSeg prev;
	(void)len;   // reserved for a debug-build boundary-fit assertion
	prev = MemSeg_Window(FAR_SEG(p));
	fn((const void*)WIN(FAR_OFS(p)), ctx);
	MemSeg_Restore(prev);
}

void Far_With(FarPtr p, u16 len, FarBorrowFn fn, void* ctx)
{
	// Atomic vs. an ISR that also touches the window page. NOTE: this assumes
	// interrupts were enabled on entry (the common main-thread case). A version
	// that preserves IFF2 (ld a,i / conditional ei) is a planned refinement;
	// callers already in a di context should use Far_With_Fast.
	__asm di __endasm;
	Far_With_Fast(p, len, fn, ctx);
	__asm ei __endasm;
}

void Far_Read(FarPtr src, void* dst, u16 n)
{
	u8*    d   = (u8*)dst;
	MemSeg seg = FAR_SEG(src);
	u16    ofs = FAR_OFS(src);

	while (n) {
		u16 avail = (u16)(MEMSEG_SIZE - ofs);       // bytes left in this segment
		u16 chunk = (n < avail) ? n : avail;
		MemSeg prev = MemSeg_Window(seg);
		const volatile u8* s = WIN(ofs);
		u16 i = chunk;
		while (i--)
			*d++ = *s++;
		MemSeg_Restore(prev);
		n   -= chunk;
		seg += 1;    // next chunk (if any) starts at the base of the next segment
		ofs  = 0;
	}
}

void Far_Write(FarPtr dst, const void* src, u16 n)
{
	const u8* s   = (const u8*)src;
	MemSeg    seg = FAR_SEG(dst);
	u16       ofs = FAR_OFS(dst);

	while (n) {
		u16 avail = (u16)(MEMSEG_SIZE - ofs);       // bytes left in this segment
		u16 chunk = (n < avail) ? n : avail;
		MemSeg prev = MemSeg_Window(seg);
		volatile u8* d = WIN(ofs);
		u16 i = chunk;
		while (i--)
			*d++ = *s++;
		MemSeg_Restore(prev);
		n   -= chunk;
		seg += 1;
		ofs  = 0;
	}
}

void Far_Set(FarPtr dst, u8 val, u16 n)
{
	MemSeg seg = FAR_SEG(dst);
	u16    ofs = FAR_OFS(dst);

	while (n) {
		u16 avail = (u16)(MEMSEG_SIZE - ofs);
		u16 chunk = (n < avail) ? n : avail;
		MemSeg prev = MemSeg_Window(seg);
		volatile u8* d = WIN(ofs);
		u16 i = chunk;
		while (i--)
			*d++ = val;
		MemSeg_Restore(prev);
		n   -= chunk;
		seg += 1;
		ofs  = 0;
	}
}

u8 Far_Peek(FarPtr p)
{
	u8 v;
	MemSeg prev = MemSeg_Window(FAR_SEG(p));
	v = *WIN(FAR_OFS(p));
	MemSeg_Restore(prev);
	return v;
}

void Far_Poke(FarPtr p, u8 val)
{
	MemSeg prev = MemSeg_Window(FAR_SEG(p));
	*WIN(FAR_OFS(p)) = val;
	MemSeg_Restore(prev);
}
