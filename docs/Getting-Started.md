# Getting Started

From nothing to a running MSX ROM. Everything here is the same toolchain the library's own tests
use — no vendored SDKs, no hidden build system.

## 1. Install the toolchain

Two packages. That's the whole list.

```sh
sudo apt install sdcc openmsx        # Debian / Ubuntu
```

- **SDCC** ≥ 4.2 — the C compiler (it also brings the assembler `sdasz80`, the linker `sdld`, and
  `makebin`, which turns the linker's output into a ROM image).
- **openMSX** — the emulator. It ships with **C-BIOS**, a free BIOS, so you can boot and test
  without owning any copyrighted machine ROMs.

Check the compiler is new enough — `--sdcccall 1` (the calling convention this library is built for)
needs SDCC 4.2 or later:

```sh
sdcc --version
```

## 2. Where your program lives

An MSX has a 64 KB address space split into **four 16 KB pages**:

| Page | Address | Holds |
|---|---|---|
| 0 | `0x0000`–`0x3FFF` | the BIOS |
| 1 | `0x4000`–`0x7FFF` | **your cartridge ROM** |
| 2 | `0x8000`–`0xBFFF` | more ROM, or RAM |
| 3 | `0xC000`–`0xFFFF` | **RAM** (your variables, the stack) |

So a 16 KB cartridge puts **code at `0x4000`** and **data at `0xC000`**. That is exactly what the
build below does. If you need more code than fits, see [Code Banking](Code-Banking.md); if you need
more *data*, see [Far Pointers](Far-Pointers.md).

## 3. Your first ROM

```c
// hello.c — fill the screen, then stop.
#include "vdp.h"

void main(void)
{
	VDP_SetMode(VDP_MODE_GRAPHIC4);     // SCREEN 5
	VDP_FillVRAM_16K(0x33, 0, 0x4000);  // paint every pixel colour 3

	for (;;) {}                         // an MSX program never "exits"
}
```

Build it:

```sh
sdcc -c -mz80 --sdcccall 1 -I lib/gen vdp.c
sdcc -c -mz80 --sdcccall 1 -I lib/gen hello.c
sdcc -o hello.ihx --code-loc 0x4020 --data-loc 0xC000 -mz80 --no-std-crt0 --sdcccall 1 \
     crt0.rel hello.rel vdp.rel
makebin -s 16384 hello.ihx hello.rom
```

Run it:

```sh
openmsx -machine C-BIOS_MSX2 -carta hello.rom
```

**`--code-loc 0x4020`, not `0x4000`:** the first 32 bytes of a cartridge are the **ROM header** —
the magic bytes `AB` plus the address the BIOS should call to start you. The crt0 puts that there.

## 4. Two things that will bite you

**Your program never returns.** There is no OS to return *to*. `main` must end in an infinite loop
(or hand control to an interrupt handler). Falling off the end of `main` crashes the machine.

**Globals need a crt0 that initialises them.** A ROM's variables live in RAM, but their initial
values live in ROM — something has to copy them across at boot, and zero the rest. A correct MSX
crt0 does this (MSXgl's `crt0_rom16.asm` calls `INIT_GLOBALS`). If yours doesn't, then

```c
static u8 counter = 5;      // reads back as garbage!
static u8 total;            // also garbage — not zero
```

…are both lies. Symptoms are baffling and intermittent. Read-only tables should be `const` anyway —
they then live in ROM and cost you no RAM at all.

## 5. How this library is tested (and how you can be too)

Every example in these docs is a real program that gets compiled, booted on an emulated MSX, and
checked. The trick is simple: the program writes a byte into RAM, and the harness reads it back out
of the emulator.

```c
volatile u8 __at(0xE000) R[8];   // a result buffer at a fixed address

R[1] = something_we_measured;
R[0] = (everything_checked_out) ? 0xA5 : 0x00;   // 0xA5 = pass
```

```sh
cd docs/examples
./run_all.sh                             # every example, on MSX2
./run_all_msx1.sh                        # the MSX1-capable subset
EX_MACHINE="C-BIOS_MSX2+" ./run_all.sh   # MSX2+
```

No screenshots, no eyeballing: a test either produces `0xA5` or it doesn't. It's a good pattern to
steal for your own project.

## Where next

- **Draw something** → [VDP Access](VDP-Access.md) (screen modes), then
  [Bitmap Drawing](Bitmap-Drawing.md).
- **Make it move** → [VBlank Sync](VBlank-Sync.md) and [Double Buffering](Double-Buffering.md).
- **Make noise** → [Sound (PSG)](Sound-PSG.md).
- **Run out of memory** → [Code Banking](Code-Banking.md), [Far Pointers](Far-Pointers.md).
