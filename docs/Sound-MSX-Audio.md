# Sound: MSX-AUDIO (`msx-audio`)

The **Y8950** — the FM chip in the Philips Music Module and Panasonic FS-CA1. It is the rare,
expensive sibling of [MSX-MUSIC](Sound-MSX-Music.md): the same FM idea, but with **full control over
every operator** instead of 15 canned instruments, plus ADPCM sample playback. Link `msx-audio.c`,
include `msx-audio.h`.

Few machines have one. Detect it, use it if it's there, and fall back to MSX-MUSIC or the PSG.

## The chip

Two I/O ports, `0xC0` (register index) and `0xC1` (data). Unlike the OPLL, the **Y8950 can be read
back** — `MSXAudio_GetRegister` genuinely returns what the chip holds, which makes it far easier to
work with (and to test).

FM synthesis here is built from **operators**: each channel has two, one modulating the other. You
shape a sound by programming the operators directly.

| Registers | What |
|---|---|
| `0x20`–`0x35` | operator: multiple, tremolo, vibrato, sustain |
| `0x40`–`0x55` | operator: total level (volume) + key-scale level |
| `0x60`–`0x75` | operator: attack / decay rates |
| `0x80`–`0x95` | operator: sustain level / release rate |
| `0xA0`–`0xA8` | channel: F-number low |
| `0xB0`–`0xB8` | channel: key-on, block, F-number high |
| `0xC0`–`0xC8` | channel: feedback + algorithm |

That is the trade: MSX-MUSIC gives you a piano in one write; MSX-AUDIO makes you *build* the piano,
and then lets you build something no OPLL can.

## API

```c
void MSXAudio_Initialize(void);
bool MSXAudio_Detect(void);                    // FALSE if no Y8950 in the machine
void MSXAudio_SetRegister(u8 reg, u8 value);
u8   MSXAudio_GetRegister(u8 reg);             // really works — the chip reads back
void MSXAudio_Mute(void);
void MSXAudio_Resume(void);
```

## Building a voice

With no canned instruments, a voice is built by writing the operator registers directly. This one
programs operator 0 of the first channel — its tone shape and its level:

```c
// fmvoice.c — build a custom FM voice on the MSX-AUDIO (Y8950).
#include "msx-audio.h"

// Program operator 0 of the first channel: its tone shape and its level.
void fmvoice_program_op0(void)
{
	MSXAudio_SetRegister(0x20, 0x21);    // op0: multiple + tremolo/vibrato flags
	MSXAudio_SetRegister(0x40, 0x1B);    // op0: key-scale level + total level
}
```

After `MSXAudio_Initialize()` / `MSXAudio_Detect()` at boot, `fmvoice_program_op0()` runs to
`R[] = a5 01 21 1b` — and the emulated Y8950's own register `0x20` also holds `0x21`, so both the
write *and* the read-back are real. *(tested: `msxaudio_01_reg.c`)*

## ADPCM: real sample playback

This is what the Y8950 is *for*, and it is the one thing no other MSX C library wraps: **4-bit ADPCM
samples played out of RAM on the cartridge**. Real drums, real voices — not an FM approximation.

The chip addresses its sample RAM in **4-byte units**, and it must be told **how much RAM the
cartridge has**, because that selects the DRAM addressing width. Get it wrong and addresses silently
wrap:

| Constant | Cartridge |
|---|---|
| `MSXAUDIO_ADPCM_RAM_64K` | stock Philips Music Module (small DRAM) |
| `MSXAUDIO_ADPCM_RAM_256K` | the common 256 KB expansion *(default)* |

Both are tested.

```c
void MSXAudio_ADPCM_SetRamMode(u8 mode);                              // do this first
void MSXAudio_ADPCM_Reset(void);
void MSXAudio_ADPCM_WriteMemory(u32 addr, const u8* data, u16 len);   // upload a sample
void MSXAudio_ADPCM_ReadMemory(u32 addr, u8* data, u16 len);          // read it back
void MSXAudio_ADPCM_Play(u32 start, u32 stop, u16 delta, u8 volume);  // play it
void MSXAudio_ADPCM_Stop(void);
```

`delta` is the **replay rate** (DELTA-N): higher plays faster and higher-pitched. `volume` is
`0`-`255` — and unlike the FM registers, here bigger *is* louder.

### Uploading and hitting a drum

Upload the sample into the cartridge's RAM once, then trigger it whenever the drum should hit:

```c
// drums.c — upload a drum sample to the MSX-AUDIO and hit it.
#include "msx-audio.h"

#define SNARE_ADDR   0x1000       // where the snare sample lives in sample RAM

// A tiny snare sample (bytes of packed 4-bit ADPCM nibbles).
static const u8 g_Snare[8] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 };

// Upload the snare into the cartridge's sample RAM at startup.
void drum_upload_snare(void)
{
	MSXAudio_ADPCM_SetRamMode(MSXAUDIO_ADPCM_RAM_256K);   // the common 256 KB board
	MSXAudio_ADPCM_WriteMemory(SNARE_ADDR, g_Snare, 8);
}

// Hit the snare: play the uploaded sample once.
void drum_hit_snare(void)
{
	MSXAudio_ADPCM_Play(SNARE_ADDR, SNARE_ADDR + 7, 0x0400, 200);
}
```

Reading the sample straight back out of the chip confirms the upload: the run passes to
`R[] = a5 01 11 88`, and the cartridge's **actual sample RAM** at `0x1000` holds
`11 22 33 44 55 66 77 88` — read back out of the emulated chip, not out of our own buffer. The same
example passes with `MSXAUDIO_ADPCM_RAM_64K`. *(tested: `msxaudio_02_adpcm.c`, both RAM sizes)*

> **Reads are pipelined.** After putting the chip in memory-read mode, the first two bytes you read
> are stale - `MSXAudio_ADPCM_ReadMemory` performs the two dummy reads for you. If you drive the
> registers yourself, don't forget them, or every sample you read back is shifted by two.

## Notes

- **Total level is inverted.** In `0x40`–`0x55`, `0` is loudest and `63` is silent — the register
  sets attenuation, not volume.
- **Always `MSXAudio_Detect()` before using it.** The Y8950 is rare; most MSX machines don't have
  one, and writing to absent ports is silent, not fatal — you'd simply ship a game with no sound.
- ADPCM playback continues in the background once started; `MSXAudio_ADPCM_Stop` halts it.

## See also

- [Sound (MSX-MUSIC)](Sound-MSX-Music.md) — the common FM chip; use it as the fallback.
- [Sound (SCC)](Sound-SCC.md), [Sound (PSG)](Sound-PSG.md).
