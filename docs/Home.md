# msxmvl — Documentation

A C library for writing **MSX1, MSX2 and MSX2+ programs** with SDCC — games, demos, disk tools.

It gives you the machine without the assembly: draw to the screen, make sound, read the keyboard,
load files. And when your program outgrows the Z80's 64 KB — which it will — it gives you a way past
that ceiling, which is the part most libraries leave you to solve yourself.

If you know some C and have never written a line of Z80, you are the person this is written for.

## New here?

1. **[Getting Started](Getting-Started.md)** — install two packages, build a ROM, run it. Ten minutes.
2. **[VDP Access](VDP-Access.md)** — screen modes: what you can draw on, and how to pick one.
3. **[Why msxmvl](Why-msxmvl.md)** — what this library is for, and what it isn't.

## Beyond 64 KB — the part that matters

A Z80 can only *see* 64 KB at once. Every ambitious MSX project eventually hits that wall, and it is
where most libraries stop and hand you the bare hardware.

| Page | Module | What it gives you |
|------|--------|-------------------|
| [Code Banking](Code-Banking.md) | `bankpack` | run **more code than fits in 64 KB**. Write plain C, list your functions in a manifest, get a MegaROM. Works on MSX1 too |
| [Far Pointers](Far-Pointers.md) | `farmem` | use **more than 64 KB of RAM**, addressed by handle — read, write, fill, peek, poke |
| [Segment Windowing](Segment-Windowing.md) | `memseg` | the layer underneath: drive the MSX2 memory mapper directly |

## Graphics

| Page | Module | What it gives you |
|------|--------|-------------------|
| [VDP Access](VDP-Access.md) | `vdp` | **screen modes**, and reading/writing video memory |
| [Bitmap Drawing](Bitmap-Drawing.md) | `draw` | pixels, lines, boxes, circles — drawn by the V9938's blitter, not the CPU |
| [Text Output](Text-Output.md) | `print` | strings, numbers, formatted text |
| [Tilemaps](Tilemaps.md) | `tile` | build a screen out of a map of tile numbers |
| [Hardware Scroll](Hardware-Scroll.md) | `scroll` | scroll a map bigger than the screen |
| [Sprite Transforms](Sprite-FX.md) | `sprite_fx` | flip and mask sprite shapes in RAM |
| [Double Buffering](Double-Buffering.md) | `display` | draw off-screen, then flip — no flicker |
| [3D Math](3D-Math.md) | `g3d` | fixed-point multiply and sin/cos tables for software 3D |

## Sound

| Page | Module | What it gives you |
|------|--------|-------------------|
| [Sound (PSG)](Sound-PSG.md) | `psg` | the 3-channel chip **every** MSX has — start here |
| [Sound (SCC)](Sound-SCC.md) | `scc` | Konami's wavetable chip: 5 channels, you design the waveform |
| [Sound (MSX-MUSIC)](Sound-MSX-Music.md) | `msx-music` | FM synthesis with 15 ready-made instruments (FM-PAC) |
| [Sound (MSX-AUDIO)](Sound-MSX-Audio.md) | `msx-audio` | the Y8950 — full FM control **and ADPCM sample playback** |
| [Sound (MoonBlaster)](Sound-MoonBlaster.md) | `moonblaster` | play MoonBlaster 1.4 songs — the original replayer, **interrupt-time-optimised** |

## Input

| Page | Module | What it gives you |
|------|--------|-------------------|
| [Keyboard Input](Keyboard-Input.md) | `input` | which keys are held, and which were just pressed |

## Disk

The two MSX-DOS file APIs are **not interchangeable** — pick deliberately. Underneath them sits raw
sector access, which needs no file system at all.

| Page | Module | What it gives you |
|------|--------|-------------------|
| [MSX-DOS 2 — file handles](MSX-DOS-2.md) | `dos` | open / read / write / close, subdirectories. **DOS 2 only** |
| [MSX-DOS 1 — FCB](MSX-DOS-1.md) | `dos` | the older CP/M-style API. Runs on **DOS 1 *and* DOS 2** |
| [Raw Sectors](Disk-Sectors.md) | `disk` | read and write sectors directly — no files, no FAT |

## Data

| Page | Module | What it gives you |
|------|--------|-------------------|
| [Memory Operations](Memory-Operations.md) | `memory` | copy and fill blocks of RAM, fast |
| [Math Utilities](Math-Utilities.md) | `math` | ÷10 and %10, bit-reverse, random numbers |
| [Fixed-Point](Fixed-Point.md) | `fixed_point` | fractions without floats (the Z80 has no FPU) |
| [String Conversion](String-Conversion.md) | `string` | numbers → text |
| [Compression](Compression.md) | `compress` | unpack RLEp data — fit more into the ROM |
| [Encryption](Encryption.md) | `crypt` | save-game passwords, obfuscated blobs |
| [Localization](Localization.md) | `localize` | one text table per language, switched at runtime |

## Program structure

| Page | Module | What it gives you |
|------|--------|-------------------|
| [State Machines](State-Machines.md) | `fsm` | title → play → game-over, with enter/tick/leave callbacks |
| [Mutexes](Mutexes.md) | `mutex` | 8 locks in one byte — keep the interrupt handler off the VDP |
| [VBlank Sync](VBlank-Sync.md) | `vsync` | wait for the screen refresh; the heartbeat of every game loop |
| [Interrupts](Interrupts.md) | `isr` | IM 2 takeover: your C handler runs **164 T** after the interrupt — no BIOS in the loop |
| [ROM Formats](ROM-Formats.md) | `crt0_rom*` | 16 / 32 / 48 KB cartridges — one flag picks the startup |

## System

| Page | Module | What it gives you |
|------|--------|-------------------|
| [Real-Time Clock](Real-Time-Clock.md) | `clock` | date and time, plus 6 bytes of battery-backed save |

## Which machines does it run on?

Every claim here is **measured** — the examples are compiled for each machine and booted on it.

| Machine | Video chip | Result |
|---|---|---|
| **MSX2** | V9938 | all examples pass |
| **MSX2+** | V9958 | all examples pass |
| **MSX1** | TMS9918 | the applicable examples pass |

On an MSX1 the count is lower for an honest reason: page flipping, the drawing blitter, GRAPHIC4 and
hardware scroll are **V9938 features that do not exist** on an MSX1, and it has no memory mapper.
Everything else — the VDP basics, PSG, keyboard, all the pure-logic modules, and **MegaROM code
banking** — runs there. Build with `-DMSX_VERSION=MSX_1`.

Two traps worth knowing:

- **`memseg` will *appear* to work on an MSX1.** It reads back a software shadow, which cheerfully
  reports a memory mapper that isn't there. `farmem`, which really touches the hardware, fails as it
  should.
- **The `MSX_VERSION` switch is compile-time.** An MSX2 build is byte-identical to one with no MSX1
  support at all — supporting MSX1 costs you nothing on MSX2.

## How this documentation is tested

**Every code sample on these pages is a real program that was compiled, booted on an emulated MSX,
and checked.** If a sample is shown here, it passed.

It also cannot drift: a guard compares every line of code in these pages against the tested example
it came from, and **fails the build** if they diverge. You cannot change a sample here without
changing the test that proves it.

```sh
cd docs/examples
./run_all.sh          # every example, on MSX2
./run_all_msx1.sh     # the MSX1-capable subset
```

Some things a headless test genuinely cannot drive, and we say so rather than shipping untested code:
`game_pawn` and `input_manager` (only meaningful on screen), `v9990` (needs a Graphics9000 cartridge),
and the thin BIOS call-through modules. The MSX-DOS and raw-sector examples *are* tested — but they
need a DOS system disk, which is copyrighted and cannot ship, so those are the only ones you cannot
re-run from a fresh clone.

## Conventions

- **Toolchain:** SDCC with `--sdcccall 1`. Nothing else — `apt install sdcc openmsx` is the whole list.
- **Types** (`types.h`): `u8 u16 u32` unsigned, `i8 i16 i32` signed, `bool`.
- **Link only what you use.** Each module is a self-contained `.c`; every page tells you which files
  to link (`vdp.c print.c`, and so on).
- **`R[]` in the examples** is the test harness's result buffer — a byte written to a fixed RAM
  address so the emulator can check it. `R[0] = 0xA5` means "all checks passed". It is not part of
  the library; ignore it when reading an example, or steal the trick for your own tests.
