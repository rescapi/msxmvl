// msxmvl EXTENSION module: memseg -- shadow-backed segment windowing.
//
// The foundation for all far-memory access (data via farmem, and later banked
// code). It owns a RAM SHADOW of the segment currently paged into the window
// page, because the mapper switch registers are write-only on hardware (the MSX
// standard specifies the RAM-mapper ports 0xFC-0xFF as write-only). Without a
// shadow there is no way to read back "what is mapped now", so a temporary
// switch could never be safely restored -- which is exactly what MemSeg_Window
// / MemSeg_Restore provide, and what nested far-access depends on.
//
// Window page = page 2 (0x8000-0xBFFF). Page 1 holds code, page 3 holds the
// stack/data; page 2 is the one safe page to repurpose as a switchable window.
//
// Backend (compile-time):
//   MEMSEG_BACKEND_RAM  (default) -- MSX2 RAM memory mapper, 16 KB segments,
//                                    selected by OUT to port 0xFE (page 2).
//   MEMSEG_BACKEND_ROM8 -- reserved for the ASCII-8 MegaROM backend (8 KB).
#ifndef MSXMVL_MEMSEG_H
#define MSXMVL_MEMSEG_H

#include "types.h"

#if !defined(MEMSEG_BACKEND_RAM) && !defined(MEMSEG_BACKEND_ROM8)
#define MEMSEG_BACKEND_RAM
#endif

#ifdef MEMSEG_BACKEND_RAM
#define MEMSEG_SIZE    0x4000u   // 16 KB RAM-mapper segment
#define MEMSEG_SHIFT   14        // log2(MEMSEG_SIZE) -- for linear far addresses
#define MEMSEG_WINDOW  0x8000u   // page 2 base
#else
#define MEMSEG_SIZE    0x2000u   // 8 KB ASCII-8 segment (ROM8, reserved)
#define MEMSEG_SHIFT   13
#define MEMSEG_WINDOW  0x8000u   // ASCII-8 bank-2 base
#endif

typedef u8 MemSeg;               // segment number (0..255)

// Impose a known window mapping and sync the shadow to it. Call once at startup.
// Because the boot segment cannot be read back, we do not try to discover it --
// we SET a known segment and record it, so shadow == hardware from then on.
// `home` should be a segment not used by pages 0/1/3 (i.e. not the stack/code).
void   MemSeg_Init(MemSeg home);

// Read the shadow: which segment is in the window page right now.
MemSeg MemSeg_GetWindow(void);

// Program the window page to `seg` and update the shadow (no save).
void   MemSeg_SetWindow(MemSeg seg);

// Map `seg` into the window page; RETURN the previous segment (from the shadow)
// so the caller can restore it. This is the primitive nested far-access needs.
MemSeg MemSeg_Window(MemSeg seg);

// Put the window page back to `prev` (as returned by MemSeg_Window).
void   MemSeg_Restore(MemSeg prev);

#endif // MSXMVL_MEMSEG_H
