# OPL4 (YMF278B) register-access timing — the numbers that matter

Sources: openMSX `src/sound/YMF278B.cc` (models these; constants cite the Yamaha
datasheet) and the YMF278B application manual (scanned PDF in this directory).
Master clock 33.8688 MHz; 1 master clock ≈ 29.5 ns. Z80\@3.579545 MHz: 1 T ≈ 279 ns.

| Operation | Busy (master clocks) | Busy (µs) | Z80 T equivalent |
|---|---|---|---|
| FM register select (A1/C4-C7 addr) | 56 | 1.65 | ~6 T |
| FM register write (data) | 56 | 1.65 | ~6 T |
| Wave register select (0x7E) | 88 | 2.60 | ~9.3 T |
| Wave register write (0x7F) | 88 | 2.60 | ~9.3 T |
| Wave memory read | 38 | 1.12 | ~4 T |
| Wave memory write | 28 | 0.83 | ~3 T |
| **Instrument load (LD, status bit 1)** | ~10000 | **~300** | **~1074 T** |

Status port (0xC4 read): bit 0 = BUSY (register write pending), bit 1 = LD
(instrument load from wave header in progress).

## What this means for a replayer on MSX

- **A 3.58 MHz Z80 cannot outrun the register floors.** The fastest possible
  back-to-back OUT sequence spaces I/O ~12 T ≈ 3.35 µs apart — above both the
  1.65 µs FM and 2.60 µs wave busy windows. Plain Z80 code needs NO inserted
  waits between register writes. (Contrast the Y8950's 35-36 T floors that the
  mb14 campaign had to pad for.)
- **The LD wait is the real one.** After pointing a wave channel at a new
  instrument header, the chip loads it for ~300 µs (~1074 T) with status bit 1
  set. Any code that writes wave registers for that channel during LD must poll
  first. This is where an optimiser could be tempted to "remove a poll loop
  that never spins in the emulator" — on OPL3-substitute emulation the status
  read returns 0 and the loop exits instantly, but on real hardware it spins.
  RULE for the mbfm campaign: status-poll structure is semantics, not slack —
  never remove or reorder polls, only move work between them.
- **R800/turboR** shrinks instruction time but MSX I/O keeps wait states;
  still, the original driver has an R800 flag — preserve its behavior.
- **Emulation caveat for the rig**: the real `moonsound` extension models all
  of the above but needs the copyrighted yrw801.rom (absent here). The
  ROM-free "OPL3 Cartridge (Moonsound compatible)" covers ports 0xC4-C7 with a
  YMF262 core: FM playback and port traces work, but OPL4 BUSY/LD status bits
  read as 0 — timing measurements exclude real-hardware poll waits. Same class
  of caveat as mb14's "openMSX models no Y8950 busy time"; handled the same
  way: measure code time in the rig, respect the table above by construction.
