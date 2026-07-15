// msxmvl clean-room reimplementation of MSXgl "draw" module.
// Public names + signatures match MSXgl exactly (drop-in replacement).
// Derived from MSXgl engine/src/draw.h (interface only) +
// reference/V9938_REFERENCE.md + standard MSX/Z80 knowledge.
//
// The draw module is a thin geometry layer on top of the V9938 command engine
// (R#32-46). Every routine ultimately programs a VDP command (LINE / LMMV /
// PSET) and lets the hardware rasterise. Because the observable result is the
// VRAM produced by identical command register values, this reimplementation is
// pixel-for-pixel comparable with real MSXgl for the box/line/fill primitives.
#ifndef MSXMVL_DRAW_H
#define MSXMVL_DRAW_H

#include "types.h"

//=============================================================================
// SDCC attribute macros (mirror MSXgl core.h)
//=============================================================================
#ifndef __FASTCALL
#define __FASTCALL		__z88dk_fastcall
#endif
#ifndef __NAKED
#define __NAKED			__naked
#endif
#ifndef __PRESERVES
#define __PRESERVES		__preserves_regs
#endif

//=============================================================================
// VDP command opcodes / logical operators (mirror MSXgl vdp_reg.h)
//=============================================================================
#ifndef VDP_CMD_LMMV
#define VDP_CMD_LMMV		0x80
#define VDP_CMD_LINE		0x70
#define VDP_CMD_PSET		0x50
#endif

#ifndef VDP_OP_IMP
#define VDP_OP_IMP			0x00
#define VDP_OP_AND			0x01
#define VDP_OP_OR			0x02
#define VDP_OP_XOR			0x03
#define VDP_OP_NOT			0x04
#define VDP_OP_TIMP			0x08
#define VDP_OP_TAND			0x09
#define VDP_OP_TOR			0x0A
#define VDP_OP_TXOR			0x0B
#define VDP_OP_TNOT			0x0C
#endif

#ifndef VDP_ARG_DIY_DOWN
#define VDP_ARG_DIY_DOWN	0
#define VDP_ARG_DIY_UP		8
#define VDP_ARG_DIX_RIGHT	0
#define VDP_ARG_DIX_LEFT	4
#define VDP_ARG_MAJ_H		0
#define VDP_ARG_MAJ_V		1
#endif

//=============================================================================
// FUNCTIONS
//=============================================================================

// Function: Draw_Line
// Draws a line (one pixel wide) from (x1,y1) to (x2,y2).
void Draw_Line(UX x1, UY y1, UX x2, UY y2, u8 color, u8 op);

// Function: Draw_LineH
// Draws an horizontal line (one pixel wide) between x1 and x2 at row y.
void Draw_LineH(UX x1, UX x2, UY y, u8 color, u8 op);

// Function: Draw_LineV
// Draws a vertical line (one pixel wide) between y1 and y2 at column x.
void Draw_LineV(UX x, UY y1, UY y2, u8 color, u8 op);

// Function: Draw_Box
// Draws a box outline (one pixel wide).
void Draw_Box(UX x1, UY y1, UX x2, UY y2, u8 color, u8 op);

// Function: Draw_FillBox
// Draws a fully filled box.
void Draw_FillBox(UX x1, UY y1, UX x2, UY y2, u8 color, u8 op);

// Function: Draw_Circle
// Draws a circle outline (one pixel wide) centred on (x,y).
void Draw_Circle(UX x, UY y, u8 radius, u8 color, u8 op);

// Function: Draw_Point
// Draws a single point. (MSXgl exposes this inline as VDP_CommandPSET; msxmvl
// provides the same public symbol as a real function so one driver links to
// either library.) The sentinel tells draw.c to emit the out-of-line body; when
// draw.c is built against MSXgl's own draw.h instead (drop-in integration), the
// sentinel is absent and MSXgl's inline definition is used, avoiding a clash.
#define MSXMVL_DRAW_POINT_OUTOFLINE 1
void Draw_Point(UX x, UY y, u8 color, u8 op);

#endif // MSXMVL_DRAW_H
