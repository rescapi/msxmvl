# Proposal: self-booting disk target (boot sector + DOS payload)

**Status:** **IMPLEMENTED** — `lib/ext/bootsector.asm`, `lib/ext/crt0_bootdisk.asm`,
`tools/mkbootdsk.sh`, and the mini-diskOS runtime `lib/ext/diskos.{c,h}` are built and
verified headless on the Philips NMS 8245 disk ROM. See **[Boot-Disk.md](Boot-Disk.md)** for
the user-facing guide and the tested examples. This document is kept as the design/research
record; a few field-tested corrections to the original plan are noted inline below.

## Why

We can build `.COM` programs (`lib/ext/crt0_dos.asm`) — but running one needs an MSX-DOS
system disk, which is copyrighted, so even our own harness treats it as local-only
(`docs/examples/run_example_dos.sh`). The classic MSX distribution format sidesteps that
entirely: a **self-booting disk** — insert, power on, game runs; no DOS on the user's disk,
nothing copyrighted on ours. Every disk-era MSX game and demo shipped this way, and it is
how msxzork (or any game built on this library) should ship. The gap is one boot sector and
one packaging script.

## What MSXgl has (survey — interface facts from `vendor/msxgl`)

- **Plain DOS targets**: `TARGET_DOS1`/`DOS2` (`crt0_dos.asm`, TPA `.COM`), `TARGET_DOS2_MAPPER`
  (`crt0_dos_mapper.asm`, DOS 2 + RAM-mapper segments), `TARGET_DOS3` (Nextor). All need a DOS
  system disk.
- **`TARGET_DOS0` "Auto boot disk"** — the closest thing: the program is an ordinary `.COM`
  renamed `BOOTDISK.COM`; a bundled C++ msxtar fork (`dsktool --dos0`) writes a 720 KB FAT12
  `.dsk` whose boot sector (`engine/src/crt0/bootsector.asm`, shipped as prebuilt bytes inside
  the tool) FCB-opens `BOOTDISK.COM` through the disk-ROM kernel's BDOS at `0xF37D`, sets the
  DTA to `0x0100`, block-reads up to 48 KB, sets `SP=0xF51F`, and jumps to `0x0100`. The crt0
  is built with `DOS_ISR=1`, i.e. the app installs its own `0x0038` interrupt handler — there
  is no DOS environment to inherit one from.
- **No raw sector-loader target**, and packaging requires their compiled tool. So: yes, MSXgl
  has a boot-disk story, and its mechanism (kernel-BDOS file load — the exact path MSXDOS.SYS
  itself boots by) is the right one to steal the *idea* of; the packaging and the boot sector
  are small enough to own in house style.

## Ground truth: the boot-sector protocol (kernel-source-verified)

- After all cartridge/extension INITs, the disk ROM reads **logical sector 0** of the boot
  drive to **`0xC000`** and **calls `0xC01E` twice**: first with **carry reset** (early probe —
  the standard sector's first instruction is `ret nc`), then, after the DOS boot environment
  is set up, with **carry set**. Only the first **256 bytes** are guaranteed at `0xC000`
  (Handbook documents `C000h–C0FFh`; the DOS 2/Nextor kernel copies exactly `100h` bytes
  "because the stack may be at 0C200h") → usable boot-code budget = bytes `0x1E–0xFF` =
  **226 bytes**; re-read sector 0 yourself if you need the other half.
- Entry registers (identical meaning in the DOS 1 kernel and DOS 2/Nextor sources):
  **A** = cold-boot flag, **DE** = address of the "page the kernel back into page 1" routine,
  **HL** = address of the disk-error-handler vector (`0xF323`, DISKVE) — point it at a handler
  before reading, or a read error re-enters the kernel. Returning (carry-clear path) falls
  through toward Disk BASIC.
- **On the carry-set call** the kernel BDOS is live at **`0xF37D`** (FCB open/read/`SETDTA`,
  plus absolute-sector `0x2F`/`0x30`); page 0 = RAM (zeroed, with `RDSLT/WRSLT/CALSLT/ENASLT/CALLF`
  jump stubs), page 1 = kernel ROM (`0xF36B` pages RAM in, `0xF368` pages the kernel back),
  pages 2–3 = RAM. This is exactly how MSXDOS.SYS is loaded to `0x0100` — the most-trodden
  boot path on every DOS 1 / DOS 2 / Nextor machine.
- **On the carry-clear call** page 0 is still main-BIOS ROM, so `PHYDIO` (`0x0144`; A=drive,
  B=count, C=media, DE=sector, HL=address, Cy=write — the same call `lib/ext/disk.c` wraps) is
  directly callable, but the environment stubs above are not installed yet and load range is
  effectively `0x8000–0xC1FF` without slot gymnastics. Demoscene loaders hijack here; we don't
  need to.
- **Format constraints**: DOS 2/Nextor kernels refuse to boot-call a sector whose first byte
  isn't `0xEB`/`0xE9` — keep the standard `EB FE 90` jump header. DOS 1 derives geometry from
  the FAT's media byte, DOS 2 from the BPB — keep BPB byte `0x15` and `FAT[0]` consistent
  (`0xF9` 720 KB / `0xF8` 360 KB). Keeping a *valid* FAT12 filesystem costs nothing and keeps
  the image host-mountable, with the payload as an ordinary replaceable file.

## What to build

1. **`lib/ext/bootsector.asm`** — flat, house-style boot sector (~60 of 226 bytes): standard
   BPB, `ret nc`, store error vector via HL, set SP high, FCB-open the payload by 8.3 name,
   `SETDTA 0x0100`, block-read, `jp 0x0100`. Carry-set/kernel-BDOS design (proven by MSXgl
   *and* by MSXDOS.SYS itself); the carry-clear/PHYDIO raw variant is documented above but not
   built until something needs it.
2. **`lib/ext/crt0_bootdisk.asm`** — sibling of `crt0_dos.asm`, same `0x0100` TPA memory model
   (existing DOS examples relink unchanged), two deltas: install a `JP 0xF37D` at `0x0005` so
   the FCB API (`dos.c`) and `DiskDOS_*` keep working without DOS; `main()` returning does a
   soft reset instead of BDOS terminate. No DOS 2 file-handle promises — FCB only.
3. **`tools/mkbootdsk.sh`** — pure bash + coreutils (the bankpack ethos; MSXgl needs a compiled
   C++ tool here): emit a 360/720 KB FAT12 `.dsk` — sector 0 = assembled boot sector with the
   payload name patched in, two FATs (payload written contiguously from cluster 2, so the
   12-bit chain is a trivial sequence), one root-dir entry, payload data.
4. **Out of scope (noted)**: bankpack-style banked programs on disk — loading segments into
   RAM-mapper pages needs a mapper probe plus a per-segment load protocol (what
   `crt0_dos_mapper` gets from DOS 2 for free). Spec separately when a >48 KB disk game exists.

## Sizes / limits

- Payload ceiling ≈ **47.7 KB** (`0x0100–0xBEFF`); above sits loader workspace/stack, and the
  boot sector itself at `0xC000`.
- **Verification item — RESOLVED**: block-reads crossing `0x4000` *do* fail via the plain
  kernel-BDOS `RDBLK`, exactly as feared (page 1 = kernel ROM during the transfer; a 24 KB test
  payload loaded correctly up to `0x3EFF` and garbage above). The **chunked fallback is now
  implemented** in `bootsector.asm` phase 2: page RAM into page 1 (`0xF36B`), then re-load from
  file-sector 31 (`0x3F00`) upward via absolute-sector reads (`0x2F`) staged through `0x8000`
  (page 2 is RAM even mid-read) and `ldir`ed down into page-1 RAM. The 24 KB payload then loads
  and runs (verified: it reads its own byte at `0x5F3B`). Staging at `0x8000` supports payloads
  whose page-1 tail stays below `0x8000` (≈32 KB); a larger image would read its page-2 portion
  directly and only stage the page-1 window — noted, not yet needed.

## Acceptance

- `mkbootdsk.sh` image boots a sentinel example (result at `0xB000`, `run_example_dos.sh`
  contract) on the DOS 1 kernel (Philips NMS 8245) **and** the DOS 2 kernel (same + `-ext
  msxdos2`) — new `docs/examples/run_example_bootdisk.sh`, local-only.
- A >16 KB payload (the limit above); an FCB read through the `0x0005` shim; the image mounts
  host-side (`diskmanipulator`/mtools) and the payload file is listable and replaceable.

## CI feasibility (corrected, field-verified)

**Locally testable on the Philips NMS 8245 (proven); public CI stays local-only.** The earlier
draft of this section implied boot disks could not be tested headless at all — that was wrong.
The 8245 machine config (`Philips_NMS_8245-16`) has a built-in disk ROM, and this repo already
boots real MSX-DOS disks on it headlessly (`docs/examples/run_example_dos.sh`). A self-booting
`.dsk` boots on it exactly the same way: insert with `-diska`, let the disk ROM run our sector 0,
break/sample RAM. `docs/examples/run_example_bootdisk.sh` does precisely this and **passes** — it
generates a `.dsk` with `tools/mkbootdsk.sh`, boots it with **no DOS and no AUTOEXEC**, and reads
the sentinel back at `0xB000` (same contract as the DOS harness, result address `0xB000`).

What stays true: the 8245 disk ROM is copyrighted and not redistributable, and C-BIOS — the only
ROM public CI may ship — has no disk ROM and its machine configs have no floppy controller, so a
`.dsk` cannot even be inserted there. So **public CI cannot run boot-disk tests**; they are
**local-only, exactly the `run_example_dos.sh` precedent** (not in `docs/examples/run_all.sh`).
The Nextor-on-IDE idea below remains an unverified future experiment, not a CI answer today.

## Field notes (corrections learned while implementing)

Four things the plan did not anticipate, all now handled in the shipped code:

1. **Runtime sector reads go through BDOS `0x2F`, not PHYDIO.** Once the payload runs, page 0 is
   RAM (the disk kernel installed the low-RAM inter-slot stubs there), so `PHYDIO` at `0x0144` is
   *not* addressable — `0x0144` reads back RAM, not the BIOS. `DiskOS_Load` therefore reads via
   the disk-ROM kernel BDOS absolute-sector call (`0x2F`) reached through the `0x0005` hook the
   crt0 installs — the same door `DiskDOS_ReadSectors` (`disk.c`) already used under DOS.
2. **The payload stack must clear the kernel work area.** `crt0_bootdisk` sets `SP = 0xE000`.
   A stack up at `0xF000`+ collides with the disk kernel's high work RAM, and the very BDOS
   calls routed through `0xF37D` then crash. `0xE000` is below the work area and above the
   payload ceiling (`0xBEFF`).
3. **`DiskDOS_Transfer` needed an IX/IY guard.** BDOS `0x2F`/`0x30` clobber IX (like the `0x1B`
   case `dos.c` documents). Unguarded, a caller holding an SDCC IX-frame local — e.g.
   `DiskOS_Load` — has it corrupted mid-read and crashes. `disk.c` now pushes IX/IY around the
   call, matching its own PHYDIO path. (Latent bug; the DOS examples never hit it because they
   called from IX-less `main`.)
4. **The boot FCB's 4-byte random-record field must be zero *and* inside the 256 bytes the ROM
   copies.** RDBLK reads all four bytes; if the 4th lands at `0xC100` (uncopied RAM), it starts
   at a garbage record and loads nothing. The FCB is positioned so `+36` sits at offset `0xFB`.

## Docs

- **[Boot-Disk.md](Boot-Disk.md)** is the built user guide: the protocol, `mkbootdsk.sh` usage,
  the manifest format, the mini-diskOS runtime, and the >16 KB story. Cross-linked from
  `ROM-Formats.md` ("shipping formats: ROM or boot disk") and `Home.md`.

## Priority + effort

Medium priority, small effort: ~60-byte boot sector, ~40-line crt0 delta, and the FAT12
emitter is the only fiddly part (~150 lines of bash; the contiguous-cluster trick keeps it
honest). High shipping leverage — this is the format a finished disk game actually releases
in — and every protocol fact above is already paid for and source-verified. Do it when the
first release needs a disk.
