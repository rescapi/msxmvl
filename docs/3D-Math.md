# 3D Math (`g3d`)

Fixed-point building blocks for software 3D and rotation on the Z80 — distilled from the
`g3d_cube` demo's inner loop, where SDCC's generic `__mulint` and a runtime divide were the
bottleneck. Pure computation: no VDP, no interrupts, no dependencies. Link `g3d.c`, include
`g3d.h`.

## Fixed-point in one paragraph

There is no FPU. Angles and trig use **Q1.7**: an `i8` where `127` represents `1.0`. To
multiply a coordinate `x` by a Q1.7 factor `f`, compute `G3D_MulS8(x, f)` (a 16-bit product)
and shift right by 7 to get back to integer units: `(G3D_MulS8(x, f) >> 7)`. Angles are a
full turn split into **256 steps** (`0..255` == `0..2π`), so a byte holds an angle and wraps
for free.

---

## `G3D_MulS8`

```c
i16 G3D_MulS8(i8 a, i8 b);
```

Signed 8×8→16-bit multiply, exact for the whole `[-128,127] × [-128,127]` domain. Implemented
with a branchless signed-offset square table (`(a+b)²/4 − (a−b)²/4`), so it is much faster than
SDCC's general `__mulint` for 8-bit operands. This is the workhorse of any fixed-point rotate
or scale.

**Example — scale a model vertex by a Q1.7 factor.** Foreshorten a coordinate by `cos(angle)` —
e.g. `cos(45°)` (`≈ 0.707`), which is angle `32` of 256, so `G3D_Cos(32) = 90` in Q1.7:

```c
// vertex.c — scale a model vertex by a Q1.7 factor using the fast 8x8 multiply.
#include "g3d.h"

#define ANGLE_45  32     // 45 degrees, as 1/8 of the 256-step turn

// Scale one coordinate by cos(angle) -- e.g. foreshortening a vertex as it turns.
i8 scale_by_cos(i8 coord, u8 angle)
{
	i16 p = G3D_MulS8(coord, G3D_Cos(angle));   // Q1.7 product, kept 16-bit
	return (i8)(p >> 7);                         // Q1.7 -> integer units
}
```

`scale_by_cos(100, ANGLE_45)` returns `70` — `100 × 0.707 ≈ 70`. *(tested: `g3d_01_mul.c`)*

> **Tip:** always keep the intermediate as `i16` before the `>> 7`. Shifting an `i8` product
> would overflow — the whole point of the 16-bit return is to hold the full result.

---

## `G3D_Sin` / `G3D_Cos`

```c
i8 G3D_Sin(u8 angle);   // angle 0..255 == 0..2*pi;  returns -127..127  (Q1.7)
i8 G3D_Cos(u8 angle);   // == G3D_Sin(angle + 64)
```

Table lookups (`round(127·sin(2π·i/256))`), no computation. Because `angle` is a `u8`, adding to
it wraps at 256 automatically — you never range-reduce. `G3D_Cos` is just `G3D_Sin` a quarter
turn ahead.

**Example — rotate a 2D point by 90°.** Spin a point about the origin — the same maths a
wireframe object runs on each vertex, once per frame:

```c
// rotate.c — spin a 2D point around the origin with Q1.7 sin/cos.
#include "g3d.h"

#define ANGLE_90  64     // 90 degrees, as a quarter of the 256-step turn

// Rotate point (x,y) about the origin by `angle`; writes the result back in place.
void rotate_point(i8* x, i8* y, u8 angle)
{
	i8 s = G3D_Sin(angle);
	i8 c = G3D_Cos(angle);
	i8 nx = (i8)((G3D_MulS8(*x, c) - G3D_MulS8(*y, s)) >> 7);
	i8 ny = (i8)((G3D_MulS8(*x, s) + G3D_MulS8(*y, c)) >> 7);
	*x = nx;
	*y = ny;
}
```

`rotate_point(&x, &y, ANGLE_90)` turns `(64, 0)` into `(0, 63)`. The `63` (not `64`) is the honest
cost of Q1.7 rounding: `64 × 127 >> 7 = 63`. *(tested: `g3d_02_rotate.c`)*

> **Why 256 steps?** A byte angle wraps naturally, and 256 entries keep the table small
> (256 bytes). For a rotating object, just increment the angle byte each frame.

## See also

- [Double Buffering](Double-Buffering.md) — draw the rotated geometry into a hidden page.
- The `g3d_cube` demo in the repo root for a full rotate → project → draw loop.
