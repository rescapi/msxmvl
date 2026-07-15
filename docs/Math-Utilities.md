# Math Utilities (`math`)

Fast integer helpers the Z80 lacks in hardware: divide/modulo by 10 for decimal output, bit
tricks, and pseudo-random generators. Link `math.c`, include `math.h`. (For fixed-point trig and
signed 8×8 multiply used in 3D, see [3D Math](3D-Math.md).)

---

## `Math_Div10` / `Math_Mod10`

```c
i8 Math_Div10(i8 val);
u8 Math_Mod10(u8 val);
```

Fast `/10` and `%10` — the pieces you use to break a number into decimal digits without invoking
a general division routine (which is large and slow on Z80). 16-bit variants
(`Math_Div10_16b` / `Math_Mod10_16b`) exist for larger values.

```c
#include "math.h"
volatile u8 __at(0xE000) R[4];

void main(void)
{
	R[1] = (u8)Math_Div10(95);     // 95 / 10 = 9
	R[2] = Math_Mod10(95);         // 95 % 10 = 5
	R[0] = (R[1] == 9 && R[2] == 5) ? 0xA5 : 0x00;
	for (;;) {}
}
```

Runs to `R[] = a5 09 05`. *(tested: `math_01_divmod.c`)*

---

## `Math_Flip`

```c
u8 Math_Flip(u8 val);   // reverse bit order: bit 0 <-> bit 7, 1 <-> 6, ...
```

Reverses the bits of a byte. The classic use is mirroring an 8-pixel tile/sprite row
horizontally (each byte is 8 pixels; reversing the bits flips the row). `Math_Flip_16b` does 16
bits.

```c
#include "math.h"
volatile u8 __at(0xE000) R[4];

void main(void)
{
	R[1] = Math_Flip(0x01);        // 0000 0001 -> 1000 0000 = 0x80
	R[2] = Math_Flip(0x0F);        // 0000 1111 -> 1111 0000 = 0xF0
	R[0] = (R[1] == 0x80 && R[2] == 0xF0) ? 0xA5 : 0x00;
	for (;;) {}
}
```

Runs to `R[] = a5 80 f0`. *(tested: `math_02_flip.c`)*

---

## `Math_GetRandom16` / `Math_GetRandom8`

```c
void Math_SetRandomSeed16(u16 seed);
u16  Math_GetRandom16(void);        // 16-bit xorshift — deterministic from the seed
void Math_SetRandomSeed8(u8 seed);
u8   Math_GetRandom8(void);         // mixes in the Z80 R register — NOT reproducible
```

Two generators with an important difference:

- **`Math_GetRandom16`** is a pure xorshift PRNG — **deterministic**: the same seed reproduces the
  same sequence. Use it for replays or procedural content you want to regenerate identically.
- **`Math_GetRandom8`** folds in the Z80 refresh register `R` for extra entropy, so it is
  **not reproducible**. Use it when you just want unpredictable bytes.

```c
#include "math.h"
volatile u8 __at(0xE000) R[4];

void main(void)
{
	Math_SetRandomSeed16(12345);
	u16 first  = Math_GetRandom16();
	u16 second = Math_GetRandom16();

	Math_SetRandomSeed16(12345);   // reseed identically...
	u16 again  = Math_GetRandom16(); // ...reproduces `first`

	R[1] = (u8)first;
	R[2] = (u8)second;
	R[0] = (again == first && second != first) ? 0xA5 : 0x00;
	for (;;) {}
}
```

Runs to `R[] = a5 af 05` — reseeding reproduced the first value, and consecutive draws differ.
*(tested: `math_03_random.c`)*

## See also

- [3D Math](3D-Math.md) — `G3D_MulS8`, `G3D_Sin`, `G3D_Cos` for fixed-point graphics.
- [String Conversion](String-Conversion.md) — turn the digits from `Div10`/`Mod10` into text.
