# MSX-DOS 1 — FCB (`dos`)

The CP/M-style file API: a **File Control Block** you own and DOS scribbles in, with data moving
through a **DTA** (disk transfer address) rather than a buffer argument.

Use it when you must run on **MSX-DOS 1**. It also works fine under DOS 2 — so if you need one
binary that runs everywhere, this is it. If you can require DOS 2, prefer the far nicer
[file handles](MSX-DOS-2.md).

Your program must be a `.COM` built for the TPA (`--code-loc 0x0100`). Link `dos.c`, include
`dos.h`; the API is gated on `DOS_USE_FCB`.

**Limits you are accepting:** 8.3 filenames, space-padded and with no dot; **no subdirectories**;
128-byte records.

## API

```c
void DOS_SetTransferAddr(void* data);      // the DTA — where data goes to/comes from
u8   DOS_CreateFCB(DOS_FCB* fcb);
u8   DOS_OpenFCB(DOS_FCB* fcb);
u8   DOS_SequentialWriteFCB(DOS_FCB* fcb);
u8   DOS_SequentialReadFCB(DOS_FCB* fcb);
u8   DOS_CloseFCB(DOS_FCB* fcb);
u8   DOS_DeleteFCB(DOS_FCB* fcb);
```

All return `0` on success.

## Reading and writing a file

Save and load run through the same `g_Fcb` and its 128-byte `g_Dta` buffer. `savegame_setup_fcb`
zeroes the FCB and sets only `Drive` and `Name`; `savegame_write` creates the file and writes the
record; `savegame_read` opens it again and reads the record straight back into the DTA:

```c
// savegame.c — save & load a record through an MSX-DOS *1* File Control Block.
#include "dos.h"

static DOS_FCB g_Fcb;
static u8      g_Dta[128];          // the disk-transfer buffer: one 128-byte record

// FCB names are 8.3, SPACE-padded, with no dot: "DOCSFCB DAT".
// Zero the whole FCB, then set only Drive and Name — DOS fills in the rest.
static void savegame_setup_fcb(void)
{
	const c8* n = "DOCSFCB DAT";
	u8 i;
	for (i = 0; i < sizeof(g_Fcb); i++)
		((u8*)&g_Fcb)[i] = 0;       // an FCB must start out zeroed
	g_Fcb.Drive = 0;                // 0 = current drive
	for (i = 0; i < 11; i++)
		g_Fcb.Name[i] = n[i];
}

// Save: create the file and write the record now sitting in the DTA. -> 0 on success.
u8 savegame_write(void)
{
	u8 cr, wr, cl;
	savegame_setup_fcb();
	DOS_SetTransferAddr(g_Dta);     // FCB calls transfer through the DTA
	cr = DOS_CreateFCB(&g_Fcb);     // leaves RecordSize = 0, i.e. 128-byte records
	wr = DOS_SequentialWriteFCB(&g_Fcb);
	cl = DOS_CloseFCB(&g_Fcb);
	return cr | wr | cl;
}

// Load: open the file and read the record back into the DTA. -> 0 on success.
u8 savegame_read(void)
{
	u8 op, rd;
	savegame_setup_fcb();           // a fresh, zeroed FCB for the read
	DOS_SetTransferAddr(g_Dta);
	op = DOS_OpenFCB(&g_Fcb);
	rd = DOS_SequentialReadFCB(&g_Fcb);
	return op | rd;
}
```

Seed two marker bytes into the DTA, save, wipe the DTA, load, and it lands
`R[] = a5 00 00 00 00 c0 de` — both calls returned success and the two bytes came back off the
disk — verified on **both** an MSX-DOS 1 and an MSX-DOS 2 disk. *(tested: `dos_02_fcb.c`)*

> **Zero the FCB, then leave DOS's fields alone.** `CreateFCB`/`OpenFCB` fill it in — including
> leaving `RecordSize` at `0`, which means "128-byte records". Helpfully setting `RecordSize = 128`
> yourself makes `DOS_SequentialWriteFCB` fail with `1`. Set only `Drive` and `Name`.

## Running the tests

Both need a **DOS system disk and a machine with a disk ROM**, which are copyrighted and cannot ship
with this repository; the harness skips cleanly if `harness/dos/` is empty. These are the only
examples in this documentation that public CI cannot run — every ROM-based example needs nothing but
openMSX and C-BIOS.

The hidden test harness parks its result buffer at **`0xB000`, not `0xE000`**: under MSX-DOS the
kernel owns high RAM, so the ROM examples' usual spot is not yours to scribble on.

## See also

- [MSX-DOS 1 (FCB)](MSX-DOS-1.md) / [MSX-DOS 2 (handles)](MSX-DOS-2.md) — the other file API.
- [Raw Sectors](Disk-Sectors.md) — below the file system: no FAT, no files, no DOS.
- [Real-Time Clock](Real-Time-Clock.md) — 6 bytes of battery-backed save with no disk at all.
