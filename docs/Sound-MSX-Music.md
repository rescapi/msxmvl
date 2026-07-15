# Sound: MSX-MUSIC (`msx-music`)

**FM synthesis** — the YM2413 (OPLL), built into many MSX2+ machines and sold as the **FM-PAC**
cartridge. Nine FM channels (or six plus five drums), and — crucially — **15 built-in instruments**
you get for free: piano, guitar, flute, synth bass. That is why FM-PAC music sounds so much richer
than [PSG](Sound-PSG.md) beeps for so little code. Link `bios.c system.c msx-music.c`, include
`msx-music.h`.

## The chip

The OPLL is reached through two I/O ports: an **index** port and a **data** port. You select a
register, then write its value. `MSXMusic_SetRegister` does both.

| Registers | What |
|---|---|
| `0x00`–`0x07` | the user-defined instrument (only instrument 0) |
| `0x0E` | rhythm/drum control |
| `0x10`–`0x18` | F-number low byte — one per channel |
| `0x20`–`0x28` | key-on, octave (block), F-number high bit |
| `0x30`–`0x38` | instrument number (high nibble) + volume (low nibble) |

Playing a note is three writes: pick the **instrument and volume** (`0x3n`), set the **pitch**
(`0x1n` + `0x2n`), and set the **key-on** bit in `0x2n`.

> **The OPLL is write-only.** The CPU cannot read a single register back — `MSXMusic_GetRegister`
> exists, but on real hardware it returns junk. Keep your own copy of anything you need to know.
> (This example is therefore verified against the emulated chip's registers, not by reading them
> from the Z80.)

## API

```c
u8   MSXMusic_Initialize(void);          // detect + reset; MSXMUSIC_NOTFOUND if absent
u8   MSXMusic_Detect(void);              // MSXMUSIC_INTERNAL / _EXTERNAL / _NOTFOUND
u8   MSXMusic_GetSlotId(void);           // where it was found
void MSXMusic_SetRegister(u8 reg, u8 value);
void MSXMusic_Mute(void);
void MSXMusic_Resume(void);
```

`MSXMusic_Initialize` really does look for the chip — internal first, then an external FM-PAC (which
it also switches on, by setting bit 0 of `0x7FF6` in the cartridge). So a `MSXMUSIC_NOTFOUND` result
is trustworthy: gate your FM music on it and fall back to the PSG.

## Playing a note

```c
#include "msx-music.h"
volatile u8 __at(0xE000) R[8];

void main(void)
{
	u8 type = MSXMusic_Initialize();     // scans the slots for an OPLL

	R[1] = type;                         // 1 = internal, 2 = external (FM-PAC)
	R[2] = MSXMusic_GetSlotId();         // where it was found

	MSXMusic_SetRegister(0x30, 0x50);    // ch0: instrument 5 (piano), max volume
	MSXMusic_SetRegister(0x10, 0xAD);    // ch0: F-number low byte
	MSXMusic_SetRegister(0x20, 0x15);    // ch0: key-ON + octave + F-number high

	R[0] = (type != MSXMUSIC_NOTFOUND) ? 0xA5 : 0x00;
	for (;;) {}
}
```

Runs to `R[] = a5 02 02` — an **external** FM-PAC found in slot 2 — and the emulated OPLL's own
register `0x30` reads back `0x50`, so the write really reached the chip.
*(tested: `msxmusic_01_note.c`, on an FM-PAC)*

## Notes

- **Instrument and volume share a register.** `0x3n` is `instrument << 4 | volume`, and **volume is
  inverted**: `0` is loudest, `15` is silent. `0x50` above is instrument 5 at full volume.
- **Key-off is not a separate call** — clear bit 4 of `0x2n` to release the note.
- `MSXMusic_Mute` / `MSXMusic_Resume` save and restore the channel registers, which is what you want
  around a pause screen.

## See also

- [Sound (PSG)](Sound-PSG.md) — the fallback when no OPLL is present.
- [Sound (SCC)](Sound-SCC.md) — Konami's wavetable alternative.
- [Sound (MSX-AUDIO)](Sound-MSX-Audio.md) — the bigger, rarer FM chip.
