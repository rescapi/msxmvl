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

**Example — scale a value by a Q1.7 factor.** Scale `x = 100` by `cos(45°)` (`≈ 0.707`):

```c
#include "g3d.h"
volatile u8 __at(0xE000) R[4];

void main(void)
{
	i8  x = 100;
	i16 p = G3D_MulS8(x, G3D_Cos(32));   // = 100 * 90 = 9000
	i8  scaled = (i8)(p >> 7);           // Q1.7 -> integer: 9000>>7 = 70

	R[1] = (u8)scaled;                   // 70 (0x46)
	R[0] = (scaled == 70) ? 0xA5 : 0x00;
	for (;;) {}
}
```

Runs to `R[] = a5 46` — `100 × 0.707 ≈ 70`. *(tested: `g3d_01_mul.c`)*

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

**Example — rotate a 2D point by 90°.** Rotate `(64, 0)` by angle 64 (a quarter of 256):

```c
#include "g3d.h"
volatile u8 __at(0xE000) R[4];

void main(void)
{
	i8 x = 64, y = 0;
	i8 s = G3D_Sin(64);          // sin(90 deg) = 127
	i8 c = G3D_Cos(64);          // cos(90 deg) = 0
	i8 nx = (i8)((G3D_MulS8(x, c) - G3D_MulS8(y, s)) >> 7);   // 0
	i8 ny = (i8)((G3D_MulS8(x, s) + G3D_MulS8(y, c)) >> 7);   // 63 (rounding)

	R[1] = (u8)nx;               // 0
	R[2] = (u8)ny;               // 63
	R[0] = (nx == 0 && ny == 63) ? 0xA5 : 0x00;
	for (;;) {}
}
```

Runs to `R[] = a5 00 3f` — `(64,0)` rotates to `(0, 63)`. The `63` (not `64`) is the honest
cost of Q1.7 rounding: `64 × 127 >> 7 = 63`. *(tested: `g3d_02_rotate.c`)*

> **Why 256 steps?** A byte angle wraps naturally, and 256 entries keep the table small
> (256 bytes). For a rotating object, just increment the angle byte each frame.

## See also

- [Double Buffering](Double-Buffering.md) — draw the rotated geometry into a hidden page.
- The `g3d_cube` demo in the repo root for a full rotate → project → draw loop.
