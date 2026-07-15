// msxmvl extension module: fixed-point 3D math primitives. NOT part of the MSXgl API.
// Pure computation — no VDP, no interrupts, no gen/ dependency. Distilled from the
// g3d_cube demo's rotate/project inner loop, where SDCC's generic __mulint and a runtime
// divide were the bottleneck. These are the reusable building blocks for software 3D /
// fixed-point graphics on the Z80.
#ifndef MSXMVL_EXT_G3D_H
#define MSXMVL_EXT_G3D_H

#include "types.h"

// Signed 8x8 -> 16-bit multiply via a branchless signed-offset square table: a*b, exact for
// all i8 inputs (the full [-128,127] x [-128,127] domain). Much faster than SDCC's generic
// __mulint for 8-bit operands. Typical use for a Q1.7 fixed-point rotate: (G3D_MulS8(x, cos) >> 7).
i16 G3D_MulS8(i8 a, i8 b);

// Sine of a 256-step angle (0..255 == 0..2*pi), returned as i8 in [-127,127] (amplitude
// 127 == 1.0 in Q1.7). Table-driven, no computation.
i8 G3D_Sin(u8 angle);

// Cosine of a 256-step angle == G3D_Sin(angle + 64).
i8 G3D_Cos(u8 angle);

extern const u16 g_G3D_MulTab[512];  // floor((k-256)^2/4): signed-offset square table
extern const i8  g_G3D_Sin[256];     // round(127*sin(2*pi*i/256))

#endif // MSXMVL_EXT_G3D_H
