# Sound: SCC (`scc`)

The Konami **SCC** — the wavetable chip in *Nemesis 2*, *Salamander*, *Metal Gear 2*. Five channels,
and unlike the [PSG](Sound-PSG.md) you define **the waveform itself**: 32 samples per channel. That
is what gives SCC music its character. Link `bios.c scc.c`, include `scc.h`.

Works on **SCC and SCC+ (SCC-I)** cartridges — both are tested below.

## The chip

The SCC lives in a cartridge, not the machine, so everything goes through **inter-slot access**: the
module maps the SCC's register window into the cartridge's page 2 (`0x9000` ← `0x3F`) and then reads
and writes `0x9800…`.

| Registers | What |
|---|---|
| `SCC_REG_WAVE_1` … `WAVE_5` | 32 bytes of waveform per channel (signed samples) |
| `SCC_REG_FREQ_1` … `FREQ_5` | 12-bit pitch (lower = higher note) |
| `SCC_REG_VOL_1` … `VOL_5` | volume, 0–15 |
| `SCC_REG_MIXER` | one enable bit per channel |

The wave table is **the only part you can read back**, which is exactly what makes the SCC testable.

## API

```c
bool SCC_Initialize(void);                        // find/select the chip; FALSE if absent
void SCC_SetSlot(u8 slotId);                      // manual slot (SCC_SLOT_USER mode)
u8   SCC_AutoDetect(void);                        // scan the slots (SCC_SLOT_AUTO mode)

void SCC_LoadWaveform(u8 channel, const u8* data);   // 32 bytes
void SCC_SetFrequency(u8 channel, u16 freq);
void SCC_SetVolume(u8 channel, u8 vol);              // 0..15
void SCC_SetMixer(u8 mix);                           // bit per channel
void SCC_Mute(void);  void SCC_Resume(void);         // Resume restores the mixer
u8   SCC_SetRegister(u8 reg, u8 value);
u8   SCC_GetRegister(u8 reg);                        // wave table only
```

## Giving a channel a voice

Because you supply the waveform, "playing a note" starts with designing the instrument. Upload the
32-byte wave table to a channel, pick a pitch, set the volume, open the mixer — and the channel now
speaks with that voice. Reading the wave table back proves the bytes reached the chip.

```c
// instrument.c — give an SCC channel its own voice, then play a note on it.
#include "scc.h"

#define VOICE_LEAD   0     // channel 0 — our lead voice

// A 32-step waveform, one signed byte per step (a crude triangle-ish ramp).
static const u8 g_Wave[32] = {
	0x00, 0x10, 0x20, 0x30, 0x40, 0x50, 0x60, 0x70,
	0x7F, 0x70, 0x60, 0x50, 0x40, 0x30, 0x20, 0x10,
	0x00, 0xF0, 0xE0, 0xD0, 0xC0, 0xB0, 0xA0, 0x90,
	0x81, 0x90, 0xA0, 0xB0, 0xC0, 0xD0, 0xE0, 0xF0,
};

// Give the lead channel our waveform and sound an A (~440 Hz) on it.
void play_lead_A(void)
{
	SCC_LoadWaveform(VOICE_LEAD, g_Wave);   // upload channel 0's 32-byte instrument
	SCC_SetFrequency(VOICE_LEAD, 0x01BC);   // ~440 Hz
	SCC_SetVolume(VOICE_LEAD, 15);          // 0..15
	SCC_SetMixer(0x01);                     // enable channel 0 only
}
```

After `SCC_Initialize()` succeeds and `play_lead_A()` runs, reading the wave table back gives
`R[] = a5 01 00 7f 81` — the ramp's `0x00` start, its `0x7F` peak and `0x81` trough — on **both** an
SCC and an SCC+ cartridge. *(tested: `scc_01_wave.c`)*

## Finding the cartridge

By default (`SCC_SLOT_MODE == SCC_SLOT_USER`) the module simply **assumes slot 2** and
`SCC_Initialize` returns `TRUE` without checking anything. If the player's SCC is somewhere else,
every write goes into the void and you get silence with no error.

Build with **`-DSCC_SLOT_MODE=SCC_SLOT_AUTO`** and `SCC_Initialize` scans the slots for real, so
`FALSE` genuinely means "no SCC in this machine". Do this once at boot, remember the answer, and
prime the lead channel while you are there:

```c
// audio_boot.c — find the SCC cartridge at startup instead of guessing its slot.
#include "scc.h"

static const u8 g_Wave[32] = { /* ...the lead voice, as above... */ };

// Detect the SCC (scanning every slot) and prime the lead channel. Returns TRUE if found.
bool audio_init_scc(void)
{
	bool found = SCC_Initialize();     // scans the slots, then selects the chip
	SCC_LoadWaveform(0, g_Wave);       // prime channel 0's instrument
	return found;
}
```

`g_SCC_SlotId` then holds where it was found. With an SCC present: `R[] = a5 01 02 7f` — found in
slot 2, and the wave table reads back `0x7F`. With **no** SCC cartridge: `R[] = 00 00 ff ff` —
`SLOT_NOTFOUND`, correctly. *(tested: `scc_02_detect.c`, both ways)*

> **Why the detection is not the obvious one.** The naive probe — turn the SCC on, write a byte to
> the wave table, read it back — **false-positives on every RAM slot**, because RAM echoes whatever
> you wrote just as happily as an SCC does. `SCC_AutoDetect` instead checks that the window
> *behaves* like an SCC: the byte must read back with SCC mode **on**, and must **not** with SCC
> mode **off**. RAM keeps echoing either way and is rejected.
>
> It writes 2 probe bytes into page 2 of each slot it tries, so call it before you put anything
> valuable at `0x9000`/`0x9800`.

## Limits

- The module drives the SCC and the SCC+ **in SCC-compatible mode**. It never enables the SCC-I's
  extended mode, so `SCC_REG_WAVE_5` is not the SCC-I's independent fifth waveform — in compatible
  mode channels 4 and 5 share a wave table, as on the original SCC.
- `SCC_GetRegister` can only read the **wave table**. Frequency, volume and mixer are write-only;
  the module keeps a mixer copy for `SCC_Resume`, but nothing else can be read back.

## See also

- [Sound (PSG)](Sound-PSG.md) — the sound chip every MSX has.
- [Code Banking](Code-Banking.md) — SCC cartridges are MegaROMs; the same mapper hardware.
