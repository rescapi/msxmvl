# MSX-DOS 2 — file handles (`dos`)

The modern file API: `open` / `read` / `write` / `close`, the way you'd expect from any other OS.
Full paths, subdirectories, real error codes. **This is what you want unless you must support
MSX-DOS 1** — in which case see [MSX-DOS 1 (FCB)](MSX-DOS-1.md).

Requires **MSX-DOS 2**. Your program must be a `.COM` built for the TPA (`--code-loc 0x0100`), not a
cartridge ROM. Link `dos.c`, include `dos.h`; the API is gated on `DOS_USE_HANDLE`.

> ### It fails *silently* on MSX-DOS 1
>
> MSX-DOS 1 has no functions `0x43`/`0x48`, but calling them does **not** raise an error. Running the
> example below on a DOS 1 disk, `DOS_WriteHandle` and `DOS_ReadHandle` both returned a perfectly
> plausible **4 bytes** — while the buffer was never touched. No error code, no `0xFF` handle. Just
> wrong data.
>
> **Check `DOS_GetVersion()` first and refuse to run on DOS 1.** Do not rely on the calls to tell you.

## API

```c
u8  DOS_OpenHandle(const c8* path, u8 mode);              // -> handle, or 0xFF on error
u8  DOS_CreateHandle(const c8* path, u8 mode, u8 attr);   // create + open
u16 DOS_ReadHandle(u8 h, void* buf, u16 size);            // -> bytes read (0xFFFF on error)
u16 DOS_WriteHandle(u8 h, const void* buf, u16 size);     // -> bytes written
u8  DOS_EnsureHandle(u8 h);                               // flush to disk
u8  DOS_CloseHandle(u8 h);
u32 DOS_SeekHandle(u8 h, i32 offset, u8 mode);
u8  DOS_Delete(const c8* path);
```

## The open mode is a DENY mask, not an access mask

This is the one that will cost you an afternoon. The mode byte does not say what you *may* do — it
says what is **forbidden**:

| Constant | Value | Meaning |
|---|---|---|
| `O_RDWR` | `0x00` | nothing denied — read *and* write |
| `O_RDONLY` | `0x01` | bit 0 = **no write** |
| `O_WRONLY` | `0x02` | bit 1 = **no read** |

Violating it gets you BDOS error **`0xC6` (`.ACCV`)** from `DOS_ReadHandle`/`DOS_WriteHandle` — *not*
from the open. The open succeeds and hands you a valid handle; the failure only surfaces later.

## A full round trip

```c
#include "dos.h"
volatile u8 __at(0xB000) R[8];

static const c8 g_Path[] = "DOCS.DAT";
static u8 g_Back[4];

void main(void)
{
	u8 h;
	u16 wr, rd;

	h  = DOS_CreateHandle(g_Path, O_RDWR, 0);   // 0 = normal attributes
	wr = DOS_WriteHandle(h, "\xDE\xAD\xBE\xEF", 4);
	DOS_EnsureHandle(h);                        // flush to disk BEFORE closing
	DOS_CloseHandle(h);

	h  = DOS_OpenHandle(g_Path, O_RDONLY);      // reopen and read it back
	rd = DOS_ReadHandle(h, g_Back, 4);
	DOS_CloseHandle(h);
	DOS_Delete(g_Path);                         // tidy up after ourselves

	R[1] = (u8)wr;         // 4 bytes written
	R[2] = (u8)rd;         // 4 bytes read
	R[3] = g_Back[0];      // 0xDE
	R[4] = g_Back[3];      // 0xEF

	R[0] = (wr == 4 && rd == 4 && g_Back[0] == 0xDE && g_Back[3] == 0xEF)
	     ? 0xA5 : 0x00;
	...
}
```

`R[] = a5 04 04 de ef` on MSX-DOS 2.20. *(tested: `dos_01_file.c`)*

**`DOS_EnsureHandle` before `DOS_CloseHandle`.** Data you have written may still be sitting in DOS's
buffers; `Ensure` is what pushes it to the disk.

## Errors

Every call records the raw BDOS code in `g_DOS_LastError` (`0` = success). Check it — the return
values (`0xFF` handle, `0xFFFF` byte count) tell you *that* something failed; only `g_DOS_LastError`
tells you *what*.

| Code | Name | Means |
|---|---|---|
| `0xC6` | `.ACCV` | access violation — see the deny mask above |
| `0xC7` | `.EOF` | read at/past end of file (never for a *partial* read) |
| `0xC4` | `.NHAND` | out of file handles |
| `0xD7` | `.NOFIL` | file not found |

## Running the tests

Both need a **DOS system disk and a machine with a disk ROM**, which are copyrighted and cannot ship
with this repository; the harness skips cleanly if `harness/dos/` is empty. These are the only
examples in this documentation that public CI cannot run — every ROM-based example needs nothing but
openMSX and C-BIOS.

The result buffer sits at **`0xB000`, not `0xE000`**: under MSX-DOS the kernel owns high RAM, so the
ROM examples' usual spot is not yours to scribble on.

## See also

- [MSX-DOS 1 (FCB)](MSX-DOS-1.md) / [MSX-DOS 2 (handles)](MSX-DOS-2.md) — the other file API.
- [Raw Sectors](Disk-Sectors.md) — below the file system: no FAT, no files, no DOS.
- [Real-Time Clock](Real-Time-Clock.md) — 6 bytes of battery-backed save with no disk at all.
