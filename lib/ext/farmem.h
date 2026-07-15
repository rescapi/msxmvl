// msxmvl EXTENSION module: farmem -- opaque far pointers over memseg.
//
// A FarPtr is a LINEAR far address: a flat byte offset into mapper space,
// far = seg * MEMSEG_SIZE + ofs. It is a single scalar (u32) rather than a
// struct, so SDCC can pass it by value, and so that Far_Add carries across a
// segment boundary with plain addition. The backend's segment size (16 KB for
// the RAM mapper, 8 KB for ASCII-8) is captured by MEMSEG_SHIFT, so the same
// far-pointer arithmetic works for either backend.
//
// Callers hold handles, never raw addresses that are only valid while a segment
// is mapped. The star of the API is Far_With: it hands a callback a REAL,
// mapped pointer valid ONLY inside the callback, then restores the previous
// segment -- making the "stale pointer into a paged-out segment" bug class
// structurally hard to hit.
#ifndef MSXMVL_FARMEM_H
#define MSXMVL_FARMEM_H

#include "types.h"
#include "memseg.h"

typedef u32 FarPtr;   // linear far address: seg << MEMSEG_SHIFT | ofs

#define FAR(seg, ofs)  ( ((FarPtr)(u8)(seg) << MEMSEG_SHIFT) + (u16)(ofs) )
#define FAR_SEG(p)     ( (MemSeg)((p) >> MEMSEG_SHIFT) )
#define FAR_OFS(p)     ( (u16)((p) & (MEMSEG_SIZE - 1)) )
#define Far_Add(p, d)  ( (FarPtr)((p) + (u32)(d)) )   // carries across segments

// Callback for Far_With: `win` is a real pointer into the window page, valid
// only for the duration of the call. `ctx` is passed through unchanged.
typedef void (*FarBorrowFn)(const void* win, void* ctx);

// Map the segment of `p`, hand the callback a real pointer to it, restore the
// previous segment on return. `len` is advisory (used by debug builds to assert
// the borrowed object fits in one segment). The di-safe variant assumes
// interrupts were ENABLED on entry; from a di context use Far_With_Fast.
void Far_With(FarPtr p, u16 len, FarBorrowFn fn, void* ctx);
void Far_With_Fast(FarPtr p, u16 len, FarBorrowFn fn, void* ctx);

// Copy `n` bytes from far memory to a local buffer, auto-splitting at each
// segment boundary (an object may straddle a boundary; subsequent chunks
// advance to the next segment).
void Far_Read(FarPtr src, void* dst, u16 n);

// Copy `n` bytes from a local buffer into far memory (symmetric to Far_Read;
// same boundary auto-split). Use to build/populate a RAM-mapper scratch buffer
// or copy an asset out of ROM into high RAM.
void Far_Write(FarPtr dst, const void* src, u16 n);

// Fill `n` bytes of far memory with `val` (boundary auto-split). Use to clear
// or initialize a scratch region that spans multiple segments.
void Far_Set(FarPtr dst, u8 val, u16 n);

// Single-byte access.
u8   Far_Peek(FarPtr p);
void Far_Poke(FarPtr p, u8 val);

#endif // MSXMVL_FARMEM_H
