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
| `vsync`   | `VDP_WaitVBlank()` — interrupt-safe vertical-blank sync primitive |
| `g3d`     | fixed-point 3D math: branchless signed 8×8 multiply, sin/cos |
| `moonblaster` | MoonBlaster 1.4 song playback (.MBM + .MBK) — the original mbplay replayer, interrupt-time-optimised |

## Testing

```
bash test/ext/run.sh <driver.c> <ram_len> "<mod.c ...>"
```

Drivers write a `0xA5` pass sentinel + a fail count into `R[0..]` at `0xE000`; the runner
reads them back headless from openMSX (and can also assert VDP registers / cycle counts).
