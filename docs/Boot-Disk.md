# Boot Disks (self-booting `.dsk` + mini-diskOS)

A **boot disk** is how disk-era MSX games shipped: insert it, power on, the game runs — no
MSX-DOS on the user's disk, nothing copyrighted on yours. You build a `.COM`-style payload (same
`0x0100` TPA model as [MSX-DOS 1](MSX-DOS-1.md)), and `tools/mkbootdsk.sh` packs it into a
720 KB (or 360 KB) FAT12 image whose **sector 0 is our own boot sector**. The disk ROM runs that
sector, it loads your payload to `0x0100` and jumps to it. This is the same path `MSXDOS.SYS`
itself boots by — see [Disk-Target.md](Disk-Target.md) for the source-verified protocol.

On top of the plain filesystem sits a tiny **mini-diskOS**: a precomputed sector index that lets
the payload pull data blobs straight off the disk by a numeric id, with **no FAT walk and no
DOS** — the file-system work is done once, at build time, by the packing tool. That is why it is
smaller and faster than opening a file through DOS.

The pieces:

| File | Role |
|---|---|
| `lib/ext/bootsector.asm` | the 512-byte sector 0 — loads the payload, sets up the sector index region |
| `lib/ext/crt0_bootdisk.asm` | C startup for the payload (`0x0100` TPA, installs a BDOS hook, soft-resets on return) |
| `tools/mkbootdsk.sh` | pure-bash FAT12 emitter: manifest → self-booting `.dsk` |
| `lib/ext/diskos.{c,h}` | runtime: `DiskOS_Load(id, dst)` reads a blob's sectors directly |

## Build one

Write a payload — an ordinary program whose `main()` is the whole show:

```c
// bootdisk_01_sentinel.c — the smallest self-booting disk program.
#include "types.h"

volatile u8 __at(0xB000) R[8];

void main(void)
{
	R[0] = 0xA5;            // "I booted from a DOS-less disk"
	R[1] = 0x42;            // a second byte, to prove it is really us
```

*(`R[]` is just the test harness's result buffer; a real payload would draw a title screen.)*

Then list it in a **manifest** — one line per entry, `<file> <kind> [arg]`:

```
PAYLOAD.BIN  load      0x0100
LEVEL1.DAT   resident
TILES.DAT    resident
```

- **`load`** — the boot payload (exactly one). Its 8.3 name is patched into the boot sector's
  file-open block; `<arg>` is its load address, which must be `0x0100`.
- **`resident`** — a data blob: written to the disk as a real file *and* indexed, so the running
  payload can fetch it by id. It stays on disk until asked for.

Each entry's runtime **id is its 0-based position in the manifest** (printed in the id map the
tool emits). Build the image:

```sh
tools/mkbootdsk.sh manifest.txt game.dsk 720     # or 360 for a 360 KB disk
```

The image is a **valid FAT12 disk**: it mounts host-side and every entry is a listable file
(`PAYLOAD.BIN`, `LEVEL1.DAT`, …). The sector index lives in the unused second half of the boot
sector — an extra region layered on top; the FAT still describes the same clusters.

## The mini-diskOS runtime

A payload pulls a blob off the disk by id — no file open, no FAT walk, no DOS:

```c
// bootdisk_02_diskos.c — boot with no DOS, then pull a data blob off the disk.
#include "diskos.h"
#include "types.h"

static u8 g_Buf[512];

volatile u8 __at(0xB000) R[8];

void main(void)
{
	u8  err;
	u16 len;

	// id 0 is this payload; id 1 is the resident data blob (see the manifest).
	len = DiskOS_Length(1);
	err = DiskOS_Load(1, g_Buf);
```

`DiskOS_Load(id, dst)` looks the id up in the precomputed index (id → start sector, sector
count, byte length) and reads those contiguous sectors straight into `dst`. `DiskOS_Length(id)`
and `DiskOS_Sectors(id)` answer from the same index. All three return 0 / `0xFF` for an unknown
id.

**Tradeoff (documented deliberately):** the index is only correct while the files stay put. Edit
a file host-side and its clusters may move, silently invalidating the index — so **rebuild the
image with `mkbootdsk.sh`** rather than editing files in place. In exchange you get a runtime
that needs no FAT parser and no DOS.

**How the read reaches the disk:** once the payload runs, page 0 is RAM, so the BIOS `PHYDIO`
entry (`0x0144`) is not addressable. `DiskOS_Load` reads through the disk-ROM kernel BDOS
absolute-sector call (`0x2F`), reached via the `JP 0xF37D` hook that `crt0_bootdisk.asm` installs
at `0x0005` — the same door `DiskDOS_ReadSectors` (see [Raw Sectors](Disk-Sectors.md)) uses under
DOS. So the FCB API in [MSX-DOS 1](MSX-DOS-1.md) works from a boot payload too, with no DOS on
the disk.

## Sizes and the >16 KB payload

The payload's flat ceiling is ≈47 KB (`0x0100`–`0xBEFF`). Loading is two-phase, because during
a kernel read page 1 (`0x4000`–`0x7FFF`) is the kernel ROM:

- **≤16 KB** (≤31 sectors) loads in one block read to `0x0100`.
- **>16 KB** triggers phase 2: the boot sector pages RAM into page 1 and re-loads the part above
  `0x4000` via absolute-sector reads staged through page 2, then leaves page 1 as RAM so the
  payload can run its own upper image. The kernel BDOS at `0xF37D` keeps working (it pages itself
  in/out per call), so `DiskOS_Load` still works in a large payload. *(Verified with a 24 KB
  payload on the 8245.)*

For programs much larger than that, don't grow one flat image — keep the resident payload small
and stream the rest as `resident` blobs via `DiskOS_Load`, or ship a MegaROM (see
[Code Banking](Code-Banking.md)).

## Testing (local-only)

Boot disks are tested exactly like the [MSX-DOS examples](MSX-DOS-1.md): headless on a real
disk-ROM machine.

```sh
cd docs/examples
./run_example_bootdisk.sh bootdisk_01_sentinel.c ""                    # boots, checks 0xA5
./run_example_bootdisk.sh bootdisk_02_diskos.c "diskos.c disk.c"       # boots + DiskOS_Load
```

This builds the payload, packs a `.dsk` with `mkbootdsk.sh`, boots it on the Philips NMS 8245
(built-in disk ROM) with **no DOS and no AUTOEXEC**, and reads the sentinel at `0xB000`. It is
**not** in `run_all.sh`: the 8245 disk ROM is copyrighted, and C-BIOS (all public CI may ship)
has no disk ROM — the same reason the DOS examples are local-only. See
[Disk-Target.md](Disk-Target.md) for the full CI verdict.
