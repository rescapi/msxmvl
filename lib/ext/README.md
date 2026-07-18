# msxmvl extensions (`lib/ext/`)

msxmvl-**original** modules — capabilities beyond the MSXgl API surface, distilled from the
`g3d_cube` demo work. These are *not* clean-room reimplementations and have no MSXgl oracle,
so each is validated by a **self-checking** on-target driver (see `test/ext/`) rather than by
differential comparison.

Each module is self-contained (drives the VDP / does pure computation directly) and can be
linked on its own, independent of the `gen/` modules.

| Module | Provides |
|--------|----------|
| `display` | SCREEN 5/6 double-buffer page management (draw hidden, flip to show) |
| `unzx0` | runtime **ZX0** decompressor — LZ-packed data into RAM (`UnZX0`, ties the reference `dzx0_standard` at 69 B / 89.8 T/byte; `UnZX0_fast`, 19% faster and still ROM-safe) or streamed to VRAM (`UnZX0_VRAM`); clean-room, measured ([Compression.md](../../docs/Compression.md)) |
| `unzx7` | runtime **ZX7** decompressor (`UnZX7`) — ZX0's predecessor, still common; clean-room, byte-exact MSX1+MSX2, ~ties the reference ([Compression.md](../../docs/Compression.md)) |
| `unpletter` | runtime **Pletter** decompressor (`UnPletter`) — the long-standing MSX-scene LZ format; clean-room, 184 B, byte-exact MSX1+MSX2 |
| `unlzsa2` | runtime **LZSA2** decompressor (`UnLZSA2`) — ROM-safe & C-callable; clean-room, byte-exact MSX1+MSX2 (prefer `unzx0` for size — see [Compression.md](../../docs/Compression.md)) |
| `vsync`   | `VDP_WaitVBlank()` — interrupt-safe vertical-blank sync primitive |
| `g3d`     | fixed-point 3D math: branchless signed 8×8 multiply, sin/cos |
| `moonblaster` | MoonBlaster 1.4 song playback (.MBM + .MBK) — the original mbplay replayer, interrupt-time-optimised |
| `isr` | IM 2 interrupt takeover — a plain C handler 164 T after the vector, BIOS bypassed |
| `sram` | battery-backed game saves: PAC/FM-PAC cartridge (any slot) or your MegaROM's own mapper-SRAM chip (bankpack `SRAM` line); `lib/gen/pac.c` is a thin MSXgl-compatible layer over its PAC core |
| `crt0_rom16/32/48` | flat single-file ROM startups (48 KB owns page 0 + the interrupt vector) |
| `bootsector` / `crt0_bootdisk` | self-booting FAT12 disk: sector 0 loads a `0x0100` payload, no DOS ([Boot-Disk.md](../../docs/Boot-Disk.md)) |
| `diskos` | mini-diskOS runtime — `DiskOS_Load(id, dst)` reads a blob off precomputed sectors, no FAT walk |

## Testing

```
bash test/ext/run.sh <driver.c> <ram_len> "<mod.c ...>"
```

Drivers write a `0xA5` pass sentinel + a fail count into `R[0..]` at `0xE000`; the runner
reads them back headless from openMSX (and can also assert VDP registers / cycle counts).
