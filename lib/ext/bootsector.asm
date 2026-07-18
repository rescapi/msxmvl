;; bootsector.asm -- msxmvl self-booting FAT12 disk boot sector (PASMO syntax).
;;
;; Assembled to a flat 512-byte sector 0 by tools/mkbootdsk.sh. Loads a payload
;; program off the disk to 0x0100 and jumps to it -- no MSX-DOS on the disk, no
;; copyrighted system files. This is the exact path MSXDOS.SYS itself boots by.
;;
;; PROTOCOL (kernel-source-verified; see docs/Disk-Target.md / docs/Boot-Disk.md):
;;   After all cartridge/extension INITs the disk ROM reads logical sector 0 to
;;   0xC000, copies the first 256 bytes there, and CALLs 0xC01E twice --
;;     1. carry CLEAR (early probe): the first instruction `ret nc` returns.
;;     2. carry SET  (boot): the kernel BDOS is live at 0xF37D; page 0 = RAM.
;;   Entry on the carry-set call: A = cold-boot flag, DE = "page the kernel back"
;;   routine, HL = disk-error vector (DISKVE, 0xF323). Only bytes 0x1E..0xFF are
;;   guaranteed present at 0xC000 (the kernel copies 0x100 bytes because its stack
;;   may sit at 0xC200) -> usable boot-code budget = 226 bytes.
;;
;;   The first byte MUST be 0xEB or 0xE9 or the DOS2/Nextor kernel refuses to boot
;;   the sector; we keep the standard `EB FE 90` jump header and a valid FAT12 BPB
;;   so the image also mounts host-side.
;;
;; TWO-PHASE LOAD (the >16 KB story, proven on the 8245):
;;   Phase 1 -- FCB-open the payload by name and RDBLK it to 0x0100. This loads
;;   the first 16 KB (0x0100..0x3EFF) correctly. During the kernel read page 1
;;   (0x4000..0x7FFF) is the kernel ROM, so bytes that would land there are lost.
;;   Phase 2 (only if the payload spans >31 sectors) -- page RAM into page 1
;;   (0xF36B), then re-load from file-sector 31 (memory 0x3F00) upward via the
;;   absolute-sector read (BDOS 0x2F) STAGED through 0x8000 (page 2 is always RAM),
;;   `ldir`ing each chunk down into the now-RAM page 1. Leaves page 1 = RAM so the
;;   payload can run and read its own >16 KB image; the kernel BDOS at 0xF37D still
;;   works (it pages itself in/out per call), so DiskOS_Load keeps working too.
;;
;; Bytes 0x100..0x1FF of the sector are NOT copied to RAM at boot; they hold the
;; mini-diskOS sector index (written by mkbootdsk.sh, read by lib/ext/diskos.c).
;; mkbootdsk patches fcb_name, bs_start_sec and bs_total_sec below.

		org	0xC000

;; ---------------------------------------------------------------------------
;; BPB -- standard FAT12, 720 KB (media 0xF9). mkbootdsk.sh overwrites bytes
;; 0x0B..0x1D for a 360 KB image; the boot code never reads the BPB itself (the
;; kernel BDOS does), so one build serves both geometries.
		db	0xEB, 0xFE, 0x90		; 0x00 required jump header
		db	"MSXMVL  "			; 0x03 OEM name (8)
		dw	512				; 0x0B bytes per sector
		db	2				; 0x0D sectors per cluster
		dw	1				; 0x0E reserved sectors (sector 0)
		db	2				; 0x10 number of FATs
		dw	112				; 0x11 root directory entries
		dw	1440				; 0x13 total sectors (720 KB)
		db	0xF9				; 0x15 media descriptor
		dw	3				; 0x16 sectors per FAT
		dw	9				; 0x18 sectors per track
		dw	2				; 0x1A heads
		dw	0				; 0x1C hidden sectors

;; ---------------------------------------------------------------------------
;; 0x1E: boot code. Reached twice; the carry-clear probe returns immediately.
FCB_RECSIZE	equ	fcb + 14		; FCB field: bytes per record
KBDOS		equ	0xF37D			; disk-ROM kernel BDOS entry
PAGE1_RAM	equ	0xF36B			; disk-ROM: page RAM into page 1

boot:
		ret	nc			; carry-clear probe -> back to the kernel

		;; carry set: install the disk-error vector (HL = DISKVE) so a read
		;; error lands in our handler instead of re-entering the kernel.
		ld	de, diskerr
		ld	(hl), e
		inc	hl
		ld	(hl), d

		ld	sp, 0xF51F		; safe high stack for the load

		;; --- phase 1: FCB-open the payload and pull it to 0x0100 ---
		ld	de, fcb
		ld	c, 0x0F			; FOPEN
		call	KBDOS
		inc	a			; 0xFF (not found) -> 0
		jp	z, diskerr

		ld	hl, 1
		ld	(FCB_RECSIZE), hl	; 1-byte records => byte-count block read

		ld	de, 0x0100
		ld	c, 0x1A			; SETDTA
		call	KBDOS

		ld	hl, 0xBE00		; max records (loads up to the 0x0100..0x3EFF part)
		ld	de, fcb
		ld	c, 0x27			; RDBLK (random block read)
		call	KBDOS

		;; --- phase 2: only for payloads spanning >31 sectors (>~16 KB) ---
		;; (a payload never exceeds ~95 sectors, so the count fits one byte)
		ld	a, (bs_total_sec)
		cp	32
		jr	nc, phase2		; >=32 sectors: stage page 1
		jp	0x0100			; <=31 sectors: fast path, run now

phase2:
		ld	hl, 0x3F00		; dest = 0x0100 + 31*512
		ld	(p2_dest), hl
		ld	hl, (bs_start_sec)
		ld	de, 31
		add	hl, de
		ld	(p2_sec), hl		; first sector still to load
		ld	hl, (bs_total_sec)
		or	a
		sbc	hl, de			; remaining = total - 31
		ld	(p2_rem), hl

p2loop:
		;; chunk = min(remaining, 16 sectors = 8 KB). remaining <= 64 (a payload
		;; is <=95 sectors), so its high byte is always 0 -- test the low byte.
		ld	a, (p2_rem)
		cp	16
		jr	c, p2set
		ld	a, 16
p2set:
		ld	(p2_cnt), a

		ld	de, 0x8000		; stage in page 2 (RAM even during the read)
		ld	c, 0x1A			; SETDTA
		call	KBDOS

		ld	de, (p2_sec)		; DE = first sector
		ld	a, (p2_cnt)
		ld	h, a			; H = sector count
		ld	l, 0			; L = drive 0
		ld	c, 0x2F			; RDABS (absolute sector read)
		call	KBDOS
		call	PAGE1_RAM		; the read pages the kernel in; get RAM back

		ld	a, (p2_cnt)		; BC = cnt * 512
		ld	h, a
		ld	l, 0
		add	hl, hl
		ld	b, h
		ld	c, l
		ld	hl, 0x8000
		ld	de, (p2_dest)
		ldir				; copy staged sectors into page-1 RAM
		ld	(p2_dest), de

		ld	hl, (p2_sec)		; sec += cnt
		ld	a, (p2_cnt)
		ld	e, a
		ld	d, 0
		add	hl, de
		ld	(p2_sec), hl

		ld	hl, (p2_rem)		; remaining -= cnt
		or	a
		sbc	hl, de
		ld	(p2_rem), hl
		ld	a, h
		or	l
		jr	nz, p2loop

		jp	0x0100			; run the payload

		;; A read error re-enters here through DISKVE; just hang so a
		;; headless run times out (a real machine would show a dead disk).
diskerr:
		di
		jr	diskerr

;; ---------------------------------------------------------------------------
;; Scratch + patch cells (all within the 256 bytes the kernel copies to 0xC000).
bs_start_sec:	dw	0		; payload's first logical sector (patched)
bs_total_sec:	dw	1		; payload length in sectors      (patched)
p2_dest:	dw	0
p2_sec:		dw	0
p2_rem:		dw	0
p2_cnt:		db	0

;; File control block. Only Drive (0 = boot drive) and the 8.3 Name are set; the
;; kernel fills the rest on FOPEN, and the record-size / random-record fields we
;; rely on start zeroed here.
fcb:
		db	0			; +0  drive (0 = default)
fcb_name:
		db	"PAYLOAD BIN"		; +1  8.3 name (patched by mkbootdsk.sh)
		ds	25, 0			; +12..+36 extent, recsize, size, random record
						; -- the +33..+36 random record MUST be zero
						; AND inside the 256 bytes the ROM copies, so
						; RDBLK starts at record 0 (see the +36 trap)
