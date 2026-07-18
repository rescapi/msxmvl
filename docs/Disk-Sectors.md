# Disk Sectors (`disk`)

Raw sector access: **no FAT, no directory, no filenames** — just *"give me sector N"*. This is the
layer below [MSX-DOS files](MSX-DOS-2.md). Use it for disk utilities (sector editors, copiers, format
tools), custom on-disk formats, and loaders that pull a level straight off a track without paying
for a file system. Link `disk.c`, include `disk.h`.

## Two doors to the same hardware — and you must pick the right one

This is the part that will waste your time if nobody tells you, so here it is up front.

| | `Disk_ReadSectors` / `Disk_WriteSectors` | `DiskDOS_ReadSectors` / `DiskDOS_WriteSectors` |
|---|---|---|
| Goes through | **BIOS `PHYDIO`** (`0x0144`) | **BDOS** `0x2F` / `0x30` (absolute sector) |
| Needs MSX-DOS? | **No** | Yes — it is a `.COM` call |
| Use from | cartridge / BASIC / any post-boot code | an MSX-DOS `.COM` |
| File system involved? | **No** | **No** — still raw sectors |

Both are raw sector I/O. The difference is only *how you reach the drive*, and it is forced on you
by where your code runs:

- **Under MSX-DOS, page 0 is RAM.** The BIOS is not mapped there, so `call 0x0144` lands in RAM and
  executes garbage. `PHYDIO` is simply not reachable from a `.COM`. Use the `DiskDOS_*` pair.
- **From a cartridge, `PHYDIO` needs the disk ROM to have initialised.** `PHYDIO` dispatches through
  a RAM hook that the *disk interface ROM installs during its own boot INIT*. A cartridge in slot 1
  gets its INIT called **before** the disk ROM in slot 3 — so if your cart never returns from INIT
  (as most game ROMs don't), that hook does not exist yet.

> **The failure is silent.** Calling `PHYDIO` before the disk ROM has initialised does not return an
> error. It returns **success** (`0`) and reads **nothing** — your buffer is left untouched. If a
> sector read "works" but every byte is zero, this is why.
>
> So `Disk_*` is for code that runs **after the boot sequence completes**: a BASIC-launched binary,
> or a cart that returns from INIT and takes control later (e.g. from a timer hook). It is *not*
> usable from a ROM that hijacks the machine in its INIT.

## API

```c
void Disk_SetDrive(u8 drive);    // 0 = A:, 1 = B:, ...
void Disk_SetMedia(u8 media);    // DISK_MEDIA_720K etc. (also byte 0x15 of the boot sector)

u8 Disk_ReadSectors    (u16 sector, u8 count, void* buffer);         // BIOS PHYDIO
u8 Disk_WriteSectors   (u16 sector, u8 count, const void* buffer);
u8 DiskDOS_ReadSectors (u16 sector, u8 count, void* buffer);         // BDOS, under MSX-DOS
u8 DiskDOS_WriteSectors(u16 sector, u8 count, const void* buffer);
```

All four return **0 on success**, otherwise an error code (`DISK_ERR_NOT_READY`,
`DISK_ERR_WRITE_PROTECT`, …). Sectors are 512 bytes, so `buffer` needs `count * 512` bytes, in RAM.

## Reading the boot sector

Sector 0 of any MSX disk begins with the jump `EB FE 90`, and byte `0x15` is the media descriptor.
Nothing here knows what a file is.

```c
// diskload.c — read a raw 512-byte sector straight off the disk, no file system.
#include "disk.h"

static u8 g_Sector[512];    // one sector = 512 bytes

// Read one sector from drive A: into g_Sector. Sector 0 is the boot sector,
// which begins with the jump EB FE 90 and carries the media byte at 0x15.
u8 load_sector(u16 sector)
{
	Disk_SetDrive(0);                             // A:
	return DiskDOS_ReadSectors(sector, 1, g_Sector);
}
```

Call `load_sector(0)` and it runs to `R[] = a5 00 eb fe 90 f8` — the boot sector's `EB FE 90` jump,
and `f8` being the real media byte of the test disk. *(tested: `disk_01_sector.c`)*

## Writing a sector

```c
// fastsave.c — a custom save format: write the save block straight to a raw sector.
#include "disk.h"

#define SAVE_SECTOR  700    // a scratch sector near the end of a 720-sector disk

static u8 g_Out[512];       // the block we save
static u8 g_In[512];        // ...and read back to verify

// Save: write one 512-byte block straight to SAVE_SECTOR. -> 0 on success.
u8 fastsave_write(void)
{
	Disk_SetDrive(0);
	return DiskDOS_WriteSectors(SAVE_SECTOR, 1, g_Out);
}

// Load: read the block straight back from SAVE_SECTOR. -> 0 on success.
u8 fastsave_read(void)
{
	Disk_SetDrive(0);
	return DiskDOS_ReadSectors(SAVE_SECTOR, 1, g_In);
}
```

Fill a 512-byte block with `00 01 02 … FF`, `fastsave_write` it, then `fastsave_read` it back into a
second buffer and it runs to `R[] = a5 00 00 00 01 ff ff`. *(tested: `disk_02_write.c`)*

**There is no safety net.** Sector writes go straight to the medium — no FAT is updated, no file is
grown, nothing stops you from overwriting the directory or the boot sector. That is the whole point,
and also the whole danger. Pick your sectors deliberately.

## Choosing sectors

A 720 KB disk is 1440 sectors of 512 bytes; the 360 KB test disk above is 720. If you are storing
your own data on an otherwise-DOS-formatted disk, either park it in sectors the FAT marks as bad, or
use a dedicated, un-formatted disk. If you want to *find* free space properly, read the boot sector
and walk the FAT yourself — all of which `DiskDOS_ReadSectors` is perfectly able to do.

## See also

- [MSX-DOS files](MSX-DOS-2.md) — the layer above: files, directories, MSX-DOS.
- [Boot Disks](Boot-Disk.md) — the layer built on top of this: a self-booting `.dsk` whose
  mini-diskOS streams data blobs by reading their precomputed sectors, no FAT walk.
- [Code Banking](Code-Banking.md) — the other way to get past the 64 KB wall, from ROM.
