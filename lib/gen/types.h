#ifndef MSXMVL_TYPES_H
#define MSXMVL_TYPES_H
typedef unsigned char u8; typedef unsigned int u16; typedef unsigned long u32;
typedef signed char i8; typedef signed int i16; typedef signed long i32;
typedef char c8; typedef unsigned char bool; typedef u16 UX; typedef u8 UY; typedef u32 VADDR;
// UY=u8 (VDP_UNIT_X16): 8-bit Y (0-255) covers the full MSX2 screen; matches MSXgl's default and
// keeps the draw/print/scroll functions byte-identical + never-slower. (Was u16; user decision 2026-07-05.)
#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif
#ifndef NULL
#define NULL ((void*)0)
#endif
#endif
