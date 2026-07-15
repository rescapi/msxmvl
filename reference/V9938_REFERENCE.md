# V9938 (MSX-VIDEO) — distilled reference for msxmvl

Easy-readable distillation of the Yamaha V9938 MSX-VIDEO Technical Data Book (Aug 1985) plus
Grauw/openMSX timing research. Full manual: `V9938_Technical_Data_Book_Aug85.pdf` (170 pp).
This doc holds the facts msxmvl's harness, static min-gap checker, and VDP primitives need.

Sources:
- Yamaha V9938 Technical Data Book, Aug 1985 (bitsavers.org/pdf/yamaha/) — the primary manual.
- Grauw, "V9938 VRAM timings" — https://map.grauw.nl/articles/vdp-vram-timing/vdp-timing.html
- openMSX "V9938 VRAM timings, part II" — https://openmsx.org/vdp-vram-timing/vdp-timing-2.html
- MSXgl "VRAM access timing" — https://aoineko.org/msxgl/index.php?title=VRAM_access_timing

═══════════════════════════════════════════════════════════════════════════════
## 1. VRAM access timing — THE critical numbers for msxmvl
═══════════════════════════════════════════════════════════════════════════════

CPU accesses VRAM through the data port. Consecutive accesses must be spaced ≥ a minimum
interval or the VDP silently drops them (garbage on real hardware; emulators may not catch it).

**Minimum interval between CPU VRAM accesses (Z80 T-states @ 3.58 MHz):**

| Condition                          | Min gap (T-states) | Notes |
|------------------------------------|:------------------:|-------|
| Screen 0 (text, 40 col)            | **20** | slowest case |
| Screens 1–8 (incl. bitmap 5–8)     | **15** | 86 VDP cyc ≈ 14.33 → 15 |
| Sprites disabled (V9938)           | < 15 (more slots)  | more access slots per line |
| **Display blanked / disabled**     | **~effectively none** | full VRAM bandwidth |
| Vertical blank window              | ~11,400 CPU cyc at full speed before active display resumes |

**Access slots per scanline** (why the above holds):
- Screen disabled: 154 slots/line (min gap ~8 cyc)
- Sprites disabled: 88 slots/line
- Sprites enabled:  31 slots/line (gaps up to ~70 cyc worst case)

**Single VRAM access = 6 VDP clock cycles.** Burst read N bytes = 2 + 4N cycles.

### Practical rules for msxmvl VDP-write primitives
- **`OUTI`/`OTIR` = 18 T/byte on MSX** (ED-prefixed, 2 M1 fetches → +2 wait). **> 15 → safe in
  all screen modes 1–8, even active display.** This is the workhorse for VRAM copy.
- **`OUT (n),A` = 12 T** (D3 nn, 1 M1 → +1 wait). **< 15 → UNSAFE during active display**;
  only safe when **blanked**. Fine for bulk fill to an off-screen / blanked buffer.
- Screen 0: everything needs ≥ 20 T.
- **msxmvl static checker rule:** for each consecutive VRAM-port access pair, require
  gap ≥ {20 if screen0 else 15} UNLESS display is provably blanked in that window.

### Implications already used in msxmvl benchmarks
- r2v/copy at 18–23 T/byte (OUTI-based): safe all modes. ✓
- fast fill at 12 T/byte (`OUT`): **blanked-only**; a 15-T-spaced variant is safe everywhere.

═══════════════════════════════════════════════════════════════════════════════
## 2. I/O ports (MSX wiring)
═══════════════════════════════════════════════════════════════════════════════

| Port | R/W | Function |
|------|-----|----------|
| 0x98 | R/W | VRAM data (auto-increment address) |
| 0x99 | W   | Register/address setup (write twice: value, then reg#|0x80 or addr-high) |
| 0x99 | R   | Status register read (which one selected by R#15) |
| 0x9A | W   | Palette (R#16 pointer, then 2 bytes/entry) |
| 0x9B | W   | Register indirect (via R#17 pointer) |

**Set VRAM write address:** `OUT 0x99, addr_low` ; `OUT 0x99, (addr_high & 0x3F) | 0x40`.
(0x40 bit = write mode; read mode omits it. For >16 KB set R#14 = address bits 16–14.)
**Set a register:** `OUT 0x99, value` ; `OUT 0x99, reg# | 0x80`.

═══════════════════════════════════════════════════════════════════════════════
## 3. Control registers R#0–R#23, R#32–46 (summary)
═══════════════════════════════════════════════════════════════════════════════

| Reg | Purpose |
|-----|---------|
| R#0 | Mode reg 1: M3–M5 mode bits; IE1/IE2 (line-int, light-pen) |
| R#1 | Mode reg 2: M1–M2; **BL (bit6=display on/off)**; IE0 (vblank int); sprite size/mag |
| R#2 | Pattern layout table base |
| R#3, R#10 | Colour table base (low/high) |
| R#4 | Pattern generator table base |
| R#5, R#11 | Sprite attribute table base (low/high) |
| R#6 | Sprite pattern generator base |
| R#7 | Border / text colour |
| R#8 | Mode reg: SPD (sprite disable bit5), etc. |
| R#9 | Mode reg: LN (192/212 lines), interlace, NT (PAL/NTSC) |
| R#13 | Blink period |
| R#14 | VRAM address bits 16–14 (bank for >16 KB access) |
| R#15 | Status register pointer (selects which S# port 0x99 reads) |
| R#16 | Palette pointer |
| R#17 | Control register pointer (for indirect via 0x9B; bit7=no auto-inc) |
| R#18 | Display adjust (screen position offset) |
| R#19 | Interrupt line |
| R#23 | Vertical scroll offset (display start line) |
| R#32–46 | **Command engine** (see §5) |

**Screen modes** (M1–M5 across R#0/R#1):
G1=SCREEN1, G2=SCREEN2, G3=SCREEN4, **G4=SCREEN5** (256×212×16), G5=SCREEN6,
G6=SCREEN7 (512×212×16), G7=SCREEN8 (256×212×256). Text=T1/T2, MultiColor=MC.

═══════════════════════════════════════════════════════════════════════════════
## 4. Status registers S#0–S#9 (read via R#15 pointer, port 0x99)
═══════════════════════════════════════════════════════════════════════════════

| S# | Contents |
|----|----------|
| S#0 | F (vblank int flag, bit7), 5S/C (sprite collision), sprite# |
| S#1 | FL (light pen), ID (VDP id: 0=9938), FH (line int flag) |
| S#2 | **CE (command exec, bit0)**, **TR (transfer ready, bit7)**, VR, HR, BD |
| S#3–6 | Column/row of sprite collision; light-pen coords |
| S#7 | Colour result of VDP POINT/logical read command |
| S#8–9 | Border X coordinate (SRCH command result) |

**Command-engine polling:** wait `S#2 bit0 (CE)=0` before issuing a new command; wait
`S#2 bit7 (TR)=1` before each CPU↔VRAM transfer byte in HMMC/LMMC.

═══════════════════════════════════════════════════════════════════════════════
## 5. Command engine (R#32–46) — MSX2 hardware acceleration
═══════════════════════════════════════════════════════════════════════════════

Registers: R#32/33 SX, R#34/35 SY, R#36/37 DX, R#38/39 DY, R#40/41 NX, R#42/43 NY,
R#44 CLR (colour/data), R#45 ARG (dir bits DIX/DIY, MXS/MXD src/dst RAM-or-VRAM),
R#46 CMD (command + logical op).

**Commands (high nibble of R#46):**

| Cmd | Code | Meaning | Per-pixel cost (VDP cyc, optimal) |
|-----|------|---------|:---------------------------------:|
| HMMC | 0xF0 | CPU→VRAM, byte units (high-speed) | transfer-bound |
| YMMM | 0xE0 | VRAM→VRAM, Y only, byte units | |
| HMMM | 0xD0 | VRAM→VRAM, byte units | **88** (64 read + 24 write) |
| HMMV | 0xC0 | VRAM fill, byte units | **48** (write-only) |
| LMMC | 0xB0 | CPU→VRAM, logical, pixel units | |
| LMCM | 0xA0 | VRAM→CPU, logical | |
| LMMM | 0x90 | VRAM→VRAM, logical, pixel units | **120** (64+32 read + 24 write) |
| LMMV | 0x80 | VRAM fill, logical, pixel units | |
| LINE | 0x70 | draw line | |
| SRCH | 0x60 | search colour on a line | |
| PSET | 0x50 | set one pixel | |
| POINT| 0x40 | read one pixel colour → S#7 | |
| STOP | 0x00 | abort running command | |

**Logical ops (low nibble):** IMP 0x0, AND 0x1, OR 0x2, XOR 0x3, NOT 0x8, +TP (transparent)
variants 0xB–0xF.

**Usage:** poll CE=0, set R#32… via R#17 auto-increment pointer, write R#46 last to trigger.
"Byte-unit" commands (HMMV/HMMM/YMMM/HMMC) are ~2× faster than logical "pixel-unit" ones but
ignore logical op / clipping — use them for plain blits/fills.

═══════════════════════════════════════════════════════════════════════════════
## 6. Quick facts for the loop / static analyzer
═══════════════════════════════════════════════════════════════════════════════
- CPU clock 3.579545 MHz. 1 T ≈ 0.2794 µs.
- MSX adds **+1 T-state per M1 (opcode) fetch** on top of textbook Z80 timing (measured &
  confirmed by msxmvl harness self-test: djnz loop 3330→3587 = +257 = M1 count).
- VDP command execution is asynchronous: issuing cost (CPU) ≠ execution cost (VDP). Benchmark
  both separately (setup-only vs +wait).
