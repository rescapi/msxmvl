### Pletter (`UnPletter`)

`void UnPletter(const u8* src, u8* dst)` — decompress a Pletter v0.5c stream into RAM.
Clean-room decoder written from the Pletter format (independent of the reference depacker).
Pletter is the long-standing MSX-scene LZ format, so much existing MSX data is Pletter-packed.

```c
#include "unpletter.h"
u8 g_Tiles[512];
UnPletter(g_PackedTiles, g_Tiles);   // g_Tiles[] holds the decompressed bytes
```

| Decoder | Code size | ROM-safe | Byte-exact MSX1+MSX2 |
|---|---:|:--:|:--:|
| `UnPletter` (ours) | 184 B | yes | yes |

Verified byte-exact against the reference `pletter` encoder over an 18-vector corpus (corners:
1-byte, all-same, incompressible, tile data, a few KB) on C-BIOS MSX2 **and** MSX1.
*(tested: `unpletter_01_roundtrip.c`)*
