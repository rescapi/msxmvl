# V9990 (E-VDP-III / GFX9000) — distilled reference for msxmvl

Timing/optimization reference for the V9990. Compiled from the Yamaha V9990 datasheet, the V9990
Programmers Manual (msxbanzai/tni), and Grauw's MSX Assembly Page. Use with `Z80_TIMING.md`.

Sources: https://map.grauw.nl/resources/video/yamaha_v9990.pdf ,
http://msxbanzai.tni.nl/v9990/manual.html , https://map.grauw.nl/resources/msx_io_ports.php

## 1. I/O ports (MSX wiring) — the access surface
| Addr | Port | Function |
|---|---|---|
| 0x60 | P#0 | **VRAM data** (auto-increment address) |
| 0x61 | P#1 | Palette data |
| 0x62 | P#2 | **Command data** (CPU↔VRAM transfer for CMMC/BMLX/BMXL etc.) |
| 0x63 | P#3 | **Register data** (auto-increments the register index) |
| 0x64 | P#4 | **Register select** (write-only). bit6 RII, bit7 WII = inhibit read/write index auto-inc |
| 0x65 | P#5 | **Status** (read-only) |
| 0x66 | P#6 | Interrupt flag |
| 0x67 | P#7 | System control (write-only) |
| 0x6F | — | V7040 superimpose (write 0 at init, wait a frame) |

## 2. Status register (P#5, port 0x65) — the poll bits
| bit | mask | meaning |
|---|---|---|
| 0 | 0x01 | **CE** command executing (wait CE=0 before a new command) |
| 1 | 0x02 | E0 |
| 2 | 0x04 | MCS |
| 4 | 0x10 | BD border detect |
| 5 | 0x20 | HR horizontal retrace |
| 6 | 0x40 | VR vertical retrace |
| 7 | 0x80 | **TR** transfer ready (poll TR=1 before each CPU↔VRAM byte via P#2) |

## 3. Register write protocol (differs from V9938!)
- V9938: data-then-reg on the SAME port 0x99 (a 1st/2nd-byte flip-flop).
- **V9990: SEPARATE ports** — write reg# to P#4 (0x64), then data to P#3 (0x63). P#3 **auto-increments
  the register index** (bit6/7 of P#4 can inhibit), so a run of consecutive registers = one P#4 write
  + a burst of P#3 writes (great for command-register upload).
- **ISR safety:** an interrupt between the P#4 select and the P#3 write(s) that itself selects a V9990
  register corrupts the index → wrap select+data(burst) in `di`/`ei` (same discipline as V9938, even
  though the mechanism differs).

## 4. VRAM access timing — key difference from V9938
- V9990 VRAM is **120 ns dual-port DRAM (100 ns in B6 mode)** with **dedicated CPU access bandwidth**.
- Unlike the V9938, CPU↔VRAM through the auto-increment data port (0x60) is **NOT display-slot-contended
  the way the V9938 is** — you can stream with `OTIR`/`OUTI` at full Z80 speed in normal modes; the
  bottleneck is the Z80 instruction, not a VDP access-slot gap.
- So the timing floor is the **Z80 side** (see Z80_TIMING.md): `out (n),a` = 12T, `outi` = 18T,
  `otir` ≈ 23T/byte. Bulk VRAM copy/fill/upload → **OTIR/OUTI block loops** (same technique that won
  on the V9938 16K/128K functions), no per-byte spacing padding needed.
- Command-engine CPU transfers (CMMC/BMLX/BMXL) via P#2 (0x62) DO need the **TR poll** (P#5 bit7)
  before each byte — that pacing is set by the command execution, not the CPU.

## 5. Command engine (commands, high nibble of the CMD register)
STOP 0x0, LMMC 0x1, LMMV 0x2, LMCM 0x3, LMMM 0x4, CMMC 0x5, CMMM 0x7, BMXL 0x8, BMLX 0x9, BMLL 0xA,
LINE 0xB, SEARCH 0xC, POINT 0xD, PSET 0xE, ADVANCE 0xF (`<<4` in the CMD byte).
- **Byte-unit VRAM↔VRAM (BMLL) / logical (LMMM)** run on the VDP asynchronously — issue then poll CE.
- **CPU→VRAM (BMLX linear / CMMC char) and VRAM→CPU (LMCM):** stream bytes through P#2 (0x62), polling
  TR before each. Execution cost depends on mode/bpp/sprites/display (openMSX models per-command LUTs).
- Command register block is uploaded via P#3 (0x63) auto-increment — one P#4 select + a P#3 burst.

## 6. Optimization cheat-sheet for msxmvl V9990 routines
- Register/command-block upload: one `out (0x64),#firstReg` then `otir`/unrolled `outi` of the block
  through 0x63 (auto-inc), wrapped in `di`/`ei`.
- VRAM fill/copy/read: `OTIR`/`INIR`/`OUT`+`djnz` block loops on port 0x60 — no display-slot padding
  needed (unlike V9938), so the V9938 wins transfer straight over.
- CPU↔VRAM command transfers: tight `poll-TR → out (0x62)` loop; keep the poll and the byte adjacent.
- Keep the fill/copy value in A and count via `djnz`/OTIR so A is never reloaded mid-loop.
