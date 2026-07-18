# ROM Formats (16 / 32 / 48 KB cartridges)

Every ROM program here boots through one of three flat, single-file startups —
`crt0_rom16.asm`, `crt0_rom32.asm`, `crt0_rom48.asm` — selected by one harness flag
(`EX_ROM=16|32|48`). Each does the full C startup contract (zero BSS, copy initial values),
plus exactly the extra duty its size needs. When even 48 KB is not enough, the next step is
[Code Banking](Code-Banking.md) — a MegaROM with paged banks.

**Shipping formats: ROM or boot disk.** A cartridge ROM is one way to ship; the other is a
**self-booting disk** — insert, power on, game runs, no MSX-DOS required. Same `.COM`-style
`0x0100` payload, packed into a bootable FAT12 `.dsk`. See [Boot Disks](Boot-Disk.md).

| Format | Address range | Extra duty at boot | Good for |
|---|---|---|---|
| 16 KB | `0x4000-0x7FFF` | none | most examples, small tools |
| 32 KB | `0x4000-0xBFFF` | enable the cart's slot in **page 2** | games: +16 KB for code and const data |
| 48 KB | `0x0000-0xBFFF` | page 2, then take over **page 0** | maximum flat ROM — with a contract |

## 32 KB: the second half is just *there*

The BIOS only enables the cartridge's slot in page 1 (where it found the header);
`crt0_rom32` enables it in page 2 as well, so `0x8000-0xBFFF` is plain addressable ROM —
const tables, graphics, music data, no banking, no copying:

```c
// bigrom.c — a 32 KB cartridge: the ROM's second half (0x8000-0xBFFF) holds data.
#include "types.h"

// A table pinned into the ROM's second half — impossible on a 16 KB cart.
__at(0x9000) const u8 g_HighTable[64] = { /* ... */ };

// Sum the table straight out of page 2.
u16 bigrom_checksum(void)
{
	u8 i; u16 sum = 0;
	for (i = 0; i < 64; ++i) sum += g_HighTable[i];
	return sum;                      // 64 entries of i*3: 3 * (63*64/2) = 6048
}
```

*(tested: `rom32_01_bigrom.c` — the checksum is asserted on emulated hardware)*

## 48 KB: page 0 too — and the BIOS is gone

`crt0_rom48` switches page 0 to the cartridge after boot. That buys 16 KB more ROM
(`0x0000-0x3FFF`, ideal for const data) and hands the program the interrupt vector itself:
`0x0038` jumps through a 3-byte RAM pointer (`_g_ISRJump`), with a default handler that just
acknowledges the VDP. The price is the contract, stated in the crt0's header:

> **After boot there are NO BIOS calls, ever.** The library drives the hardware directly
> anyway (VDP/PSG/PPI), so most modules are fine — anything documented as calling the BIOS
> is not.

```c
// page0.c — a 48 KB cartridge: pages 0, 1 AND 2 are ROM; the BIOS is gone.
#include "types.h"

// Const data living in PAGE 0 — impossible while the BIOS occupies it.
__at(0x0100) const u8 g_Page0Table[64] = { /* ... */ };
```

*(tested: `rom48_01_page0.c` — asserts page 0 really is cart ROM: `0x0000` holds our stub
opcode where the BIOS has `di`, the vector at `0x0038` is ours, the page-0 checksum is right,
and the program survives frames of interrupts under its own default handler)*

Two hardware notes, both documented in the crt0 sources: page-0 switching writes the primary
slot register directly (BIOS `ENASLT` cannot replace the page it executes from), which covers
non-expanded cartridge slots — the common case and what emulated `carta` slots are; and a
48 KB image starts at address 0, so emulators need to be told (`-romtype page012` in openMSX —
the harness does this).

## Custom interrupts

The 48 KB RAM vector gives you IM 1 control on that format; for a portable, faster path on
*any* format, see [Interrupts](Interrupts.md) — the `isr` module's IM 2 takeover.
