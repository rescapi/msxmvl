// msxmvl EXTENSION module: sram -- battery-backed game saves (see sram.h).
//
// PAC protocol + mapper-SRAM select values: docs/SRAM.md (openMSX device
// sources are the emulation contract; verified on the openMSX `pac` extension
// and the ASCII8SRAM*/ASCII16SRAM* -romtypes by docs/examples/sram_01_pac.c
// and test/banksram/).
#include "sram.h"

#ifdef SRAM_BACKEND_PAC
//=============================================================================
// PAC / FM-PAC backend: 8 KB SRAM in a FOREIGN slot, magic-byte lock
//=============================================================================

#define PAC_SRAM_BASE  0x4000u  // SRAM occupies 0x4000-0x5FFD in the PAC's slot
#define PAC_MAGIC_1    0x5FFEu  // write 'M' here...
#define PAC_MAGIC_2    0x5FFFu  // ...then 'i' here to unlock; anything else relocks

u8 g_Sram_Slot = SLOT_NOTFOUND;

// ---- interslot primitives (BIOS RDSLT/WRSLT -- C-BIOS has both) ------------
// Signatures are shaped so --sdcccall 1 lands every argument in the register
// the BIOS wants: no stack traffic, no IY. Both BIOS calls return with
// interrupts DISABLED (documented quirk), hence the ei.

// slot+value packed in one u16 (slot high, value low): DE = slotval, E = value.
#define PV(slot, val)  (u16)(((u16)(slot) << 8) | (u8)(val))

static u8 rdslt(u16 addr, u16 slot) __naked
{
	(void)addr; (void)slot;         // HL = addr, DE = slot
	__asm
		ld   a, e          ; A = slot
		call 0x000C        ; RDSLT (A=slot, HL=addr) -> A; does its own DI
		ei
		ret                ; value in A
	__endasm;
}

static void wrslt(u16 addr, u16 slotval) __naked
{
	(void)addr; (void)slotval;      // HL = addr, DE = slot:value
	__asm
		ld   a, d          ; A = slot (E = value, where WRSLT wants it)
		call 0x0014        ; WRSLT (A=slot, HL=addr, E=value); does its own DI
		ei
		ret
	__endasm;
}

// ---- shared PAC core --------------------------------------------------------

// Raw unlock/relock. The resting state of a PAC is LOCKED -- every block call
// below relocks on the way out, so a crash mid-game can't scribble on saves.
void Sram_PacActivate(u8 slot, u8 enable)
{
	if (enable)
	{
		wrslt(PAC_MAGIC_1, PV(slot, 'M'));
		wrslt(PAC_MAGIC_2, PV(slot, 'i'));
	}
	else
		wrslt(PAC_MAGIC_2, PV(slot, 0));
}

// Is `slot` a PAC? The probe byte must NOT stick while locked (rejects RAM),
// the magic bytes must read back as written (rejects ROM/open bus), and the
// probe MUST stick while unlocked (rejects lookalikes). All three touched
// bytes are restored, and a detected PAC is left LOCKED.
bool Sram_PacTest(u8 slot)
{
	u8 fd, fe, ff, probe, ok = 0;

	fd = rdslt(0x5FFD, slot);
	fe = rdslt(PAC_MAGIC_1, slot);
	ff = rdslt(PAC_MAGIC_2, slot);
	probe = (u8)(fd ^ 0xFF);

	wrslt(PAC_MAGIC_2, PV(slot, 0));            // force locked
	wrslt(0x5FFD, PV(slot, probe));
	if (rdslt(0x5FFD, slot) == probe)
		wrslt(0x5FFD, PV(slot, fd));            // writable while locked = RAM, not a PAC
	else
	{
		Sram_PacActivate(slot, TRUE);
		if (rdslt(PAC_MAGIC_1, slot) == 'M' && rdslt(PAC_MAGIC_2, slot) == 'i')
		{
			wrslt(0x5FFD, PV(slot, probe));
			ok = (rdslt(0x5FFD, slot) == probe);
			wrslt(0x5FFD, PV(slot, fd));        // restore the data byte (still unlocked)
		}
	}
	wrslt(PAC_MAGIC_1, PV(slot, fe));           // restore the magic bytes (relocks:
	wrslt(PAC_MAGIC_2, PV(slot, ff));           // a locked PAC read them as not-'M'/'i')
	if (ok)
		wrslt(PAC_MAGIC_2, PV(slot, 0));        // safety rule: leave a found PAC locked
	return ok;
}

// Scan every (sub)slot; fill up to `max` PAC slot IDs, return how many.
u8 Sram_PacScan(u8* slots, u8 max)
{
	u8 ps, ss, exp, slot, n = 0;
	for (ps = 0; ps < 4 && n < max; ++ps)
	{
		exp = ((volatile const u8*)0xFCC1)[ps] & 0x80;   // EXPTBL: primary expanded?
		for (ss = 0; ss < 4 && n < max; ++ss)
		{
			slot = exp ? (u8)(0x80 | (ss << 2) | ps) : ps;
			if (Sram_PacTest(slot))
				slots[n++] = slot;
			if (!exp)
				break;
		}
	}
	return n;
}

void Sram_PacRead(u8 slot, u16 addr, u8* dst, u16 n)
{
	Sram_PacActivate(slot, TRUE);
	while (n--)
		*dst++ = rdslt(addr++, slot);
	Sram_PacActivate(slot, FALSE);
}

void Sram_PacWrite(u8 slot, u16 addr, const u8* src, u16 n)
{
	Sram_PacActivate(slot, TRUE);
	while (n--)
		wrslt(addr++, PV(slot, *src++));
	Sram_PacActivate(slot, FALSE);
}

void Sram_PacFill(u8 slot, u16 addr, u8 val, u16 n)
{
	Sram_PacActivate(slot, TRUE);
	while (n--)
		wrslt(addr++, PV(slot, val));
	Sram_PacActivate(slot, FALSE);
}

bool Sram_PacIsAll(u8 slot, u16 addr, u8 val, u16 n)
{
	u8 ok = 1;
	Sram_PacActivate(slot, TRUE);
	while (n--)
		if (rdslt(addr++, slot) != val) { ok = 0; break; }
	Sram_PacActivate(slot, FALSE);
	return ok;
}

// ---- block API --------------------------------------------------------------

// Reject (never clamp) a span that does not fit: a partial save is worse than
// none. Also covers "Sram_Detect not called / nothing found".
static u8 sram_bad(u16 ofs, u16 n)
{
	return (g_Sram_Slot == SLOT_NOTFOUND) || (n > SRAM_PAC_BYTES) || (ofs > SRAM_PAC_BYTES - n);
}

bool Sram_Detect(void)
{
	u8 slot;
	g_Sram_Slot = Sram_PacScan(&slot, 1) ? slot : SLOT_NOTFOUND;
	return g_Sram_Slot != SLOT_NOTFOUND;
}

u16 Sram_Size(void)
{
	return SRAM_PAC_BYTES;
}

void Sram_Read(u16 ofs, void* dst, u16 n)
{
	if (sram_bad(ofs, n))
		return;
	Sram_PacRead(g_Sram_Slot, PAC_SRAM_BASE + ofs, (u8*)dst, n);
}

void Sram_Write(u16 ofs, const void* src, u16 n)
{
	if (sram_bad(ofs, n))
		return;
	Sram_PacWrite(g_Sram_Slot, PAC_SRAM_BASE + ofs, (const u8*)src, n);
}

bool Sram_Verify(u16 ofs, const void* src, u16 n)
{
	const u8* s = (const u8*)src;
	u16 a = PAC_SRAM_BASE + ofs;
	u8 ok = 1;
	if (sram_bad(ofs, n))
		return FALSE;
	Sram_PacActivate(g_Sram_Slot, TRUE);
	while (n--)
		if (rdslt(a++, g_Sram_Slot) != *s++) { ok = 0; break; }
	Sram_PacActivate(g_Sram_Slot, FALSE);
	return ok;
}

#else // SRAM_BACKEND_MAPPER
//=============================================================================
// Mapper-SRAM backend: an SRAM segment behind YOUR OWN cartridge's bank-select
// register. Constants come from bankpack's `SRAM <kb>` manifest line (far.h).
//=============================================================================

#ifndef SRAM_SELECT
#include "far.h"    // bankpack --gen emits SRAM_SELECT/SRAM_SIZE/SRAM_SHADOW/SRAM_BANKREG here
#endif
#if !defined(SRAM_SELECT) || !defined(SRAM_SIZE) || !defined(SRAM_SHADOW) || !defined(SRAM_BANKREG)
#error "SRAM_BACKEND_MAPPER needs SRAM_SELECT/SRAM_SIZE/SRAM_SHADOW/SRAM_BANKREG -- add an `SRAM <kb>` line to the bankpack manifest (bankpack --gen emits them into far.h)"
#endif

#define SRAM_WIN       ((volatile u8*)0x8000)         // the page-2 window
#define sram_shadow    (*(volatile u8*)SRAM_SHADOW)   // segment now in the window
#define sram_bankreg   (*(volatile u8*)SRAM_BANKREG)  // window select (write-only)

// The di/select/.../restore/ei bracket around every access: interrupts stay
// OFF while SRAM is mapped -- an ISR far-calling through the same window would
// otherwise land in SRAM instead of its code. The previous segment comes from
// the SHADOW byte (the memseg discipline: the register itself is write-only),
// and only the register is touched, so the shadow stays true throughout.

static u8 sram_bad(u16 ofs, u16 n)
{
	return (n > SRAM_SIZE) || (ofs > SRAM_SIZE - n);
}

// Copy helper shaped for --sdcccall 1: dst in HL, src in DE, n on the stack.
static void sram_ldir(u16 dst, u16 src, u16 n) __naked
{
	(void)dst; (void)src; (void)n;
	__asm
		ex   de, hl        ; DE = dst, HL = src
		pop  af            ; AF = return address (slid out of the way)
		pop  bc            ; BC = n
		push af
		ld   a, b          ; n == 0 would ldir 64 KB -- refuse
		or   a, c
		ret  z
		ldir
		ret
	__endasm;
}

bool Sram_Detect(void)
{
	u8 prev, old, ok;
	__asm__("di");
	prev = sram_shadow;
	sram_bankreg = SRAM_SELECT;
	old = SRAM_WIN[0];
	SRAM_WIN[0] = (u8)(old ^ 0x5A);
	ok = (SRAM_WIN[0] == (u8)(old ^ 0x5A));   // ROM ignores the write; SRAM keeps it
	SRAM_WIN[0] = old;
	sram_bankreg = prev;
	__asm__("ei");
	return ok;
}

u16 Sram_Size(void)
{
	return SRAM_SIZE;
}

#if (SRAM_SIZE > 0x2000)
// 32 KB ASCII8 SRAM: 8 KB blocks behind the low select bits -- copy per block.
#define SRAM_BLK(ofs)   (u8)(SRAM_SELECT | ((ofs) >> 13))
#define SRAM_ADR(ofs)   ((u16)(SRAM_WIN) + ((ofs) & 0x1FFFu))
#define SRAM_LEFT(ofs)  (0x2000u - ((ofs) & 0x1FFFu))

static void sram_xfer(u16 ofs, u16 ram, u16 n, u8 to_sram)
{
	u8 prev;
	u16 in;
	while (n)
	{
		in = SRAM_LEFT(ofs);
		if (in > n)
			in = n;
		__asm__("di");
		prev = sram_shadow;
		sram_bankreg = SRAM_BLK(ofs);
		if (to_sram)
			sram_ldir(SRAM_ADR(ofs), ram, in);
		else
			sram_ldir(ram, SRAM_ADR(ofs), in);
		sram_bankreg = prev;
		__asm__("ei");
		ofs += in; ram += in; n -= in;
	}
}

void Sram_Read (u16 ofs, void* dst, u16 n)       { if (!sram_bad(ofs, n)) sram_xfer(ofs, (u16)dst, n, 0); }
void Sram_Write(u16 ofs, const void* src, u16 n) { if (!sram_bad(ofs, n)) sram_xfer(ofs, (u16)src, n, 1); }

bool Sram_Verify(u16 ofs, const void* src, u16 n)
{
	const u8* s = (const u8*)src;
	u8 prev, ok = 1;
	u16 in, i;
	const volatile u8* w;
	if (sram_bad(ofs, n))
		return FALSE;
	while (n && ok)
	{
		in = SRAM_LEFT(ofs);
		if (in > n)
			in = n;
		__asm__("di");
		prev = sram_shadow;
		sram_bankreg = SRAM_BLK(ofs);
		w = (const volatile u8*)SRAM_ADR(ofs);
		for (i = 0; i < in; ++i)
			if (w[i] != s[i]) { ok = 0; break; }
		sram_bankreg = prev;
		__asm__("ei");
		ofs += in; s += in; n -= in;
	}
	return ok;
}

#else
// 2/8 KB SRAM: one block, one bracket. (ASCII16 mirrors the chip through its
// 16 KB window, so window+ofs is right there too.)

void Sram_Read(u16 ofs, void* dst, u16 n)
{
	u8 prev;
	if (sram_bad(ofs, n) || !n)
		return;
	__asm__("di");
	prev = sram_shadow;
	sram_bankreg = SRAM_SELECT;
	sram_ldir((u16)dst, (u16)SRAM_WIN + ofs, n);
	sram_bankreg = prev;
	__asm__("ei");
}

void Sram_Write(u16 ofs, const void* src, u16 n)
{
	u8 prev;
	if (sram_bad(ofs, n) || !n)
		return;
	__asm__("di");
	prev = sram_shadow;
	sram_bankreg = SRAM_SELECT;
	sram_ldir((u16)SRAM_WIN + ofs, (u16)src, n);
	sram_bankreg = prev;
	__asm__("ei");
}

bool Sram_Verify(u16 ofs, const void* src, u16 n)
{
	const u8* s = (const u8*)src;
	const volatile u8* w = SRAM_WIN + ofs;
	u8 prev, ok = 1;
	if (sram_bad(ofs, n))
		return FALSE;
	__asm__("di");
	prev = sram_shadow;
	sram_bankreg = SRAM_SELECT;
	while (n--)
		if (*w++ != *s++) { ok = 0; break; }
	sram_bankreg = prev;
	__asm__("ei");
	return ok;
}

#endif // SRAM_SIZE > 0x2000

#endif // backend
