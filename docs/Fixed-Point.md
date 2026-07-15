# Fixed-Point Numbers (`fixed_point`)

Represent fractional values without a floating-point unit. Link `fixed_point.c`, include
`fixed_point.h`. The module is **almost entirely compile-time macros** (format IDs and
SET/GET/FRAC helpers) plus four runtime converters `QMN_Set8/Get8/Set16/Get16`.

## Qm.n formats

A fixed-point number is named **Qm.n**: `m` integer bits and `n` fraction bits. The header
defines a constant and helper macros for each layout — e.g. `Q7_1` (7 integer + 1 fraction, range
−64…63.5), `Q4_4`, `Q8_8`, etc. Storing a value in Qm.n scales it by `2^n`; reading it back
divides by `2^n`. Fractions are exact powers of two, and all the math is plain integer add / shift.

---

## `QMN_Set8` / `QMN_Get8`

```c
i8 QMN_Set8(u16 q, i8 a);   // pack integer `a` into fixed-point format `q`
i8 QMN_Get8(u16 q, i8 a);   // unpack a fixed-point value back to an integer
```

`Set` packs, `Get` unpacks, given a format ID (`Q7_1`, `Q6_2`, …). 16-bit variants
(`QMN_Set16` / `QMN_Get16`) cover wider ranges. There are also compile-time macros
(`Q7_1_SET(a)`, `Q7_1_GET(a)`, `Q7_1_FRAC(a)`) when the format is fixed and you want zero
call overhead.

```c
#include "fixed_point.h"
volatile u8 __at(0xE000) R[4];

void main(void)
{
	i8 packed = QMN_Set8(Q7_1, 10);        // 10 in Q7.1 -> 20
	i8 back    = QMN_Get8(Q7_1, packed);   // 20 -> 10

	R[1] = (u8)packed;                     // 20
	R[2] = (u8)back;                       // 10
	R[0] = (packed == 20 && back == 10) ? 0xA5 : 0x00;
	for (;;) {}
}
```

Runs to `R[] = a5 14 0a` — `10` packs to `20` in Q7.1 and unpacks back to `10`. *(tested:
`fixed_01_qmn.c`)*

> **Why fixed-point?** Smooth sub-pixel movement, velocities, and easing without floats. Keep a
> position in, say, Q8.8, add a fractional velocity each frame, and take the integer part
> (`QMN_Get`) for the on-screen pixel.

## See also

- [3D Math](3D-Math.md) — `G3D_MulS8` and the Q1.7 sine/cosine tables use exactly this idea for
  rotation.
