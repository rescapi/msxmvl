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

Rendering a score to the HUD, you peel off one decimal digit at a time — `Mod10` gives the
current ones digit, `Div10` shifts the rest down for the next one:

```c
// hud.c — turn a score into decimal digits for the on-screen display.
#include "math.h"

// The ones digit of a score value (0..9) — the next character to draw.
u8 score_ones(u8 score)
{
	return Math_Mod10(score);
}

// Drop the ones digit, leaving the tens-and-up part still to be drawn.
u8 score_shift_down(u8 score)
{
	return (u8)Math_Div10(score);
}
```

For a score of `95`, `score_ones` returns `5` and `score_shift_down` returns `9` — repeat until
the value reaches `0` and you have every digit. *(tested: `math_01_divmod.c`)*

---

## `Math_Flip`

```c
u8 Math_Flip(u8 val);   // reverse bit order: bit 0 <-> bit 7, 1 <-> 6, ...
```

Reverses the bits of a byte. The classic use is mirroring an 8-pixel tile/sprite row
horizontally (each byte is 8 pixels; reversing the bits flips the row). `Math_Flip_16b` does 16
bits.

Mirroring a sprite so the hero can face the other way, each byte is 8 pixels; reversing its bits
flips that row left-to-right — a left-facing frame from the right-facing artwork, for free:

```c
// sprite.c — mirror an 8-pixel sprite row so the hero can face the other way.
#include "math.h"

// Return one sprite/tile row mirrored horizontally.
u8 mirror_row(u8 row)
{
	return Math_Flip(row);
}
```

`mirror_row(0x01)` (`0000 0001`) becomes `0x80` (`1000 0000`), and `mirror_row(0x0F)` becomes
`0xF0` — the row reflected. *(tested: `math_02_flip.c`)*

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

Seed a level's enemy spawns once, then draw a value per spawn. Reseed with the same number and the
whole wave replays identically — the basis for a deterministic replay:

```c
// spawn.c — pick enemy spawns from a seed you can reproduce for replays.
#include "math.h"

// Start a level's spawn sequence from a known seed.
void spawn_seed(u16 seed)
{
	Math_SetRandomSeed16(seed);
}

// Draw the next spawn value (e.g. which enemy to place, or where).
u16 spawn_next(void)
{
	return Math_GetRandom16();
}
```

Seed with `12345`, and two `spawn_next()` draws differ; reseed with `12345` and the first draw
comes back the same — reproducible when you need it. *(tested: `math_03_random.c`)*

## See also

- [3D Math](3D-Math.md) — `G3D_MulS8`, `G3D_Sin`, `G3D_Cos` for fixed-point graphics.
- [String Conversion](String-Conversion.md) — turn the digits from `Div10`/`Mod10` into text.
