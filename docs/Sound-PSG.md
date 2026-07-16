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
`PSG_SetVolume(chan, vol)` writes its volume register (channel A → register 8). A one-shot sound
effect — a coin pickup, a jump blip — is nothing more than that: a tone at a volume on a channel
you keep free for effects.

```c
// sfx.c — one-shot sound effects on the PSG (a coin pickup, a jump blip).
#include "psg.h"

#define SFX_CHANNEL   0        // channel A, kept free for sound effects

// Start a sound effect: a tone at a given pitch and volume on the effects channel.
void sfx_play(u16 period, u8 volume)
{
	PSG_SetTone(SFX_CHANNEL, period);
	PSG_SetVolume(SFX_CHANNEL, volume);
}
```

`sfx_play(0x123, 12)` writes period `0x123` (fine `0x23`, coarse `0x01`) and volume `12`; reading
registers 0, 1 and 8 back gives `R[] = a5 23 01 0c` — exactly what was written. To actually *hear*
it you also enable the channel in the mixer (`PSG_SetMixer` / `PSG_EnableTone`). *(tested:
`psg_01_tone.c`)*

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
