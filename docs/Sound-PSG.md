# Sound (`psg`)

Drive the PSG (AY-3-8910) — the MSX's 3-channel tone + noise sound chip. Link `psg.c`, include
`psg.h`. The module wraps the 14 PSG registers in typed setters so you rarely poke registers by
number.

## The chip in brief

Three channels (A/B/C), each with a **12-bit tone period** (lower period = higher pitch), a
shared **noise** generator, a **mixer** enabling tone and/or noise per channel, and a per-channel
**volume** (0–15, or "use the envelope"). One **envelope** generator shapes volume over time.

---

## `PSG_SetTone` / `PSG_SetVolume` / `PSG_GetRegister`

```c
void PSG_SetTone(u8 chan, u16 period);   // channel pitch (12-bit period)
void PSG_SetVolume(u8 chan, u8 vol);      // 0..15
u8   PSG_GetRegister(u8 reg);             // read any PSG register back
```

`PSG_SetTone(chan, period)` writes the channel's two period registers (channel A → registers 0/1);
`PSG_SetVolume(chan, vol)` writes its volume register (channel A → register 8). To actually hear
it you also enable the channel in the mixer (`PSG_SetMixer` / `PSG_EnableTone`).

```c
#include "psg.h"
volatile u8 __at(0xE000) R[8];

void main(void)
{
	PSG_SetTone(0, 0x123);         // channel A period 0x123 -> reg0=0x23, reg1=0x01
	PSG_SetVolume(0, 12);          // channel A volume -> reg8 = 12

	R[1] = PSG_GetRegister(0);     // 0x23 (fine period)
	R[2] = PSG_GetRegister(1);     // 0x01 (coarse period)
	R[3] = PSG_GetRegister(8);     // 12   (volume)
	R[0] = (R[1] == 0x23 && R[2] == 0x01 && R[3] == 12) ? 0xA5 : 0x00;
	for (;;) {}
}
```

Runs to `R[] = a5 23 01 0c` — the tone period and volume read back exactly as written.
*(tested: `psg_01_tone.c`)*

### Making an audible note

```c
PSG_SetTone(0, period);        // pitch
PSG_SetMixer(0x3E);            // enable tone on channel A only (bit clear = on)
PSG_SetVolume(0, 13);          // audible level
```

## Beyond the basics

`psg.h` also provides `PSG_SetNoise`, `PSG_SetEnvelope` / `PSG_SetShape` (envelope-driven volume),
`PSG_EnableTone` / `PSG_EnableNoise` / `PSG_EnableEnvelope`, and `PSG_Mute` / `PSG_Resume`. They
follow the same "typed setter over the register" pattern shown here.

## See also

- [VBlank Sync](VBlank-Sync.md) — update music/SFX once per frame from a `VDP_WaitVBlank`-paced loop.
