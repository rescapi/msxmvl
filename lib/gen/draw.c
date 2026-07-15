// msxmvl clean-room reimplementation of MSXgl "draw" module.
// Toolchain: SDCC --sdcccall 1
//   arg1 -> HL(16b)/A(8b), arg2 -> DE(16b)/E(8b), arg3+ -> stack.
//
// The header declares no __PRESERVES() contract on the Draw_* functions, so
// they follow the standard SDCC convention (AF/BC/DE/HL/IX caller-clobberable;
// IY left untouched). Each routine builds a V9938 command block for registers
// R#36..R#46 and hands it to SendCmd36(), which waits for the previous command
// to finish then streams the 11 bytes through the R#17 auto-increment pointer.
//
// Port map (reference/V9938_REFERENCE.md sect. 2):
//   0x99 W : register/address setup   (value, then reg#|0x80)
//   0x99 R : status (register selected by R#15)
//   0x9B W : indirect register write via R#17 pointer
//
// Command engine (sect. 5): R#36/37 DX, R#38/39 DY, R#40/41 NX, R#42/43 NY,
//   R#44 CLR, R#45 ARG, R#46 CMD. Writing R#46 triggers execution, so it must
//   be last -- the auto-increment stream guarantees that ordering.

#include "draw.h"

//=============================================================================
// COMMAND BLOCK (matches VDP_Command36 layout: 11 bytes, no padding on z80)
//=============================================================================
typedef struct
{
	u16 DX;  // R#36-37
	u16 DY;  // R#38-39
	u16 NX;  // R#40-41
	u16 NY;  // R#42-43
	u8  CLR; // R#44
	u8  ARG; // R#45
	u8  CMD; // R#46
} DrawCmd;

//-----------------------------------------------------------------------------
// Wait for the command engine to be idle (S#2 bit0 CE == 0), then load
// registers R#36..R#46 from the 11-byte block pointed to by HL and trigger.
static void SendCmd36(const DrawCmd* cmd) __FASTCALL __NAKED
{
	cmd; // in HL
	// ISR safety: every 2-write pair to port 0x99 and the R#36..R#46 burst must be
	// atomic. The MSX interrupt handler drives the VDP too, so an interrupt landing
	// between the two halves of a pair (or mid-burst) leaves the VDP's address
	// flip-flop / register pointer desynchronised and the command is corrupted.
	// Same discipline as VDP_CommandWait / VDP_CommandCustomR36 in vdp.c.
	__asm
		; --- wait for previous command to finish (poll S#2 CE) ---
		; The status-register pointer (R#15) is GLOBAL state and the MSX interrupt
		; handler reads S#0, leaving R#15 = 0. So the poll must not run with
		; interrupts enabled while S#2 is selected -- it would silently read S#0,
		; whose bit0 is not CE, and mistake a busy engine for an idle one.
		; Select once and spin with DI held (one IN per iteration, so a back-to-back
		; command stream like Draw_Circle stays cheap), and only if the engine is
		; still busy after a full 256-iteration budget do we restore S#0, open an
		; interrupt window, and start over. That bounds interrupt latency without
		; paying a 4-OUT re-select on every spin.
	00001$:
		di
		ld   a, #2               ; select status register S#2 ...
		out  (0x99), a
		ld   a, #(0x80 | 15)     ; ... via R#15
		out  (0x99), a
		ld   b, #0               ; spin budget (256)
	00002$:
		in   a, (0x99)           ; read S#2
		rra                      ; CE (bit0) -> carry
		jr   nc, 00003$          ; CE clear => engine idle, keep DI and proceed
		djnz 00002$
		; budget spent and still busy: give interrupts a window, then retry.
		xor  a
		out  (0x99), a
		ld   a, #(0x80 | 15)
		out  (0x99), a           ; R#15 = 0 for the ISR
		ei
		jr   00001$
	00003$:
		; Still inside DI, S#2 selected. R#15 is reset below, before the EI.
		; --- point R#17 at R#36 with auto-increment enabled (bit7=0) ---
		ld   a, #36
		out  (0x99), a
		ld   a, #(0x80 | 17)
		out  (0x99), a
		; --- stream 11 register bytes to the indirect data port 0x9B ---
		ld   c, #0x9B
		.rept 11
			outi                 ; OUT (C),(HL); INC HL  (16cc/byte vs otir 21)
		.endm
		; restore the status pointer the BIOS ISR expects, then release
		xor  a
		out  (0x99), a
		ld   a, #(0x80 | 15)
		ei                       ; EI defers acceptance one instruction ...
		out  (0x99), a           ; ... so this final write is still atomic
		ret
	__endasm;
}

//-----------------------------------------------------------------------------
// Fast repeat-PSET: NX/NY/CLR/ARG stay latched in the VDP between commands, so a
// run of PSETs at the same colour only has to re-upload DX/DY (R#36..R#39) and
// re-trigger by writing R#46. That is 4 OUTI + one register write instead of the
// full 11-byte block -- ~112 T-states saved per plotted point. The caller MUST have
// primed all seven fields with one SendCmd36 first.
// The circle plotter emits 8 points back-to-back. Selecting S#2 to poll the command
// engine, restoring S#0 for the BIOS ISR, and toggling di/ei cost ~86 T *per point* if
// done per point -- so hoist all three out and amortise them across the whole octet.
// SendPsetBegin/End bracket the octet; SendPsetPoint runs with interrupts already
// disabled and R#15 already == 2.
//
// The DI window spans 8 points (~2 kT, well under a frame), so V-blank is never missed.

static void SendPsetBegin(void) __NAKED
{
	__asm
		di
		ld   a, #2
		out  (0x99), a
		ld   a, #(0x80 | 15)    ; R#15 = 2 -> status port reads S#2 (CE bit)
		out  (0x99), a
		ret
	__endasm;
}

static void SendPsetEnd(void) __NAKED
{
	__asm
		xor  a
		out  (0x99), a
		ld   a, #(0x80 | 15)    ; R#15 = 0 -> S#0, what the default BIOS ISR acks with
		ei                      ; EI defers acceptance one instruction ...
		out  (0x99), a          ; ... so this final write is still atomic
		ret
	__endasm;
}

// Precondition: interrupts disabled, R#15 == 2. Writes DX/DY then retriggers R#46.
static void SendPsetPoint(const DrawCmd* cmd) __FASTCALL __NAKED
{
	cmd; // in HL
	__asm
	00001$:
		ld   b, #0               ; spin budget (256)
	00002$:
		in   a, (0x99)
		rra                      ; CE (bit0) -> carry
		jr   nc, 00003$
		djnz 00002$
		; Budget exhausted: hand the CPU back so pending interrupts can run, with the
		; status pointer the ISR expects, then re-enter the wait.
		xor  a
		out  (0x99), a
		ld   a, #(0x80 | 15)
		out  (0x99), a
		ei
		nop                      ; one instruction of interrupt window
		di
		ld   a, #2
		out  (0x99), a
		ld   a, #(0x80 | 15)
		out  (0x99), a
		jr   00001$
	00003$:
		; R#17 -> R#36, auto-increment on
		ld   a, #36
		out  (0x99), a
		ld   a, #(0x80 | 17)
		out  (0x99), a
		ld   c, #0x9B
		.rept 4
			outi                 ; DX lo/hi, DY lo/hi   (HL -> cmd+4)
		.endm
		; point R#17 at R#46 and write CMD (offset 10) to trigger execution
		ld   de, #6
		add  hl, de              ; HL -> cmd->CMD
		ld   a, #46
		out  (0x99), a
		ld   a, #(0x80 | 17)
		out  (0x99), a
		ld   a, (hl)
		out  (0x9B), a           ; writing R#46 starts the command
		ret
	__endasm;
}

//=============================================================================
// PUBLIC DRAW ROUTINES
//=============================================================================

//-----------------------------------------------------------------------------
// Draw_Line: rasterise via the hardware LINE command.
// NX holds the long-side delta, NY the short-side delta; ARG.MAJ selects which
// physical axis is the major one, and DIX/DIY give the travel direction.
void Draw_Line(UX x1, UY y1, UX x2, UY y2, u8 color, u8 op)
{
	DrawCmd cmd;
	u16 dx, dy;
	u8 arg = 0;

	if (x2 >= x1)
		dx = x2 - x1;
	else
	{
		dx = x1 - x2;
		arg |= VDP_ARG_DIX_LEFT;
	}

	if (y2 >= y1)
		dy = y2 - y1;
	else
	{
		dy = y1 - y2;
		arg |= VDP_ARG_DIY_UP;
	}

	cmd.DX = x1;
	cmd.DY = y1;
	if (dx >= dy)               // major axis is X
	{
		cmd.NX = dx;
		cmd.NY = dy;
		arg |= VDP_ARG_MAJ_H;
	}
	else                        // major axis is Y
	{
		cmd.NX = dy;
		cmd.NY = dx;
		arg |= VDP_ARG_MAJ_V;
	}
	cmd.CLR = color;
	cmd.ARG = arg;
	cmd.CMD = VDP_CMD_LINE | op;
	SendCmd36(&cmd);
}

//-----------------------------------------------------------------------------
// Draw_LineH: 1-pixel-high logical fill.
void Draw_LineH(UX x1, UX x2, UY y, u8 color, u8 op)
{
	DrawCmd cmd;

	if (x2 >= x1)
	{
		cmd.DX = x1;
		cmd.NX = x2 - x1 + 1;
	}
	else
	{
		cmd.DX = x2;
		cmd.NX = x1 - x2 + 1;
	}
	cmd.DY = y;
	cmd.NY = 1;
	cmd.CLR = color;
	cmd.ARG = 0;
	cmd.CMD = VDP_CMD_LMMV | op;
	SendCmd36(&cmd);
}

//-----------------------------------------------------------------------------
// Draw_LineV: 1-pixel-wide logical fill.
void Draw_LineV(UX x, UY y1, UY y2, u8 color, u8 op)
{
	DrawCmd cmd;

	cmd.DX = x;
	if (y2 >= y1)
	{
		cmd.DY = y1;
		cmd.NY = y2 - y1 + 1;
	}
	else
	{
		cmd.DY = y2;
		cmd.NY = y1 - y2 + 1;
	}
	cmd.NX = 1;
	cmd.CLR = color;
	cmd.ARG = 0;
	cmd.CMD = VDP_CMD_LMMV | op;
	SendCmd36(&cmd);
}

//-----------------------------------------------------------------------------
// Draw_Box: four edges (two horizontal, two vertical).
void Draw_Box(UX x1, UY y1, UX x2, UY y2, u8 color, u8 op)
{
	Draw_LineH(x1, x2, y1, color, op);   // top
	Draw_LineH(x1, x2, y2, color, op);   // bottom
	Draw_LineV(x1, y1, y2, color, op);   // left
	Draw_LineV(x2, y1, y2, color, op);   // right
}

//-----------------------------------------------------------------------------
// Draw_FillBox: single logical fill covering the whole rectangle (inclusive).
void Draw_FillBox(UX x1, UY y1, UX x2, UY y2, u8 color, u8 op)
{
	DrawCmd cmd;

	// Matches MSXgl's contract: (x1,y1) is the top-left source, (x2,y2) the bottom-right
	// destination (x1<=x2, y1<=y2). No corner normalization (MSXgl doesn't do it either).
	cmd.DX = x1;
	cmd.NX = x2 - x1 + 1;
	cmd.DY = y1;
	cmd.NY = y2 - y1 + 1;
	cmd.CLR = color;
	cmd.ARG = 0;
	cmd.CMD = VDP_CMD_LMMV | op;
	SendCmd36(&cmd);
}

//-----------------------------------------------------------------------------
// Draw_Point: one pixel via the PSET command.
// Only emitted for standalone msxmvl (its draw.h declares Draw_Point out-of-line
// and sets the sentinel). Under MSXgl's draw.h, Draw_Point is inline there.
#ifdef MSXMVL_DRAW_POINT_OUTOFLINE
void Draw_Point(UX x, UY y, u8 color, u8 op)
{
	DrawCmd cmd;

	cmd.DX = x;
	cmd.DY = y;
	cmd.NX = 1;
	cmd.NY = 1;
	cmd.CLR = color;
	cmd.ARG = 0;
	cmd.CMD = VDP_CMD_PSET | op;
	SendCmd36(&cmd);
}
#endif

//-----------------------------------------------------------------------------
// Draw_Circle: integer midpoint circle. Each iteration plots the 8 octant-
// symmetric points with a PSET command. Coordinates that underflow past 0 wrap
// to a large u16 and are clipped away by the VDP, matching the unclipped
// geometry MSXgl feeds the command engine.
// Plot the 8 octant reflections of (ox,oy) around (x,y). The constant command
// fields (NX/NY/CLR/ARG/CMD) are set once by the caller; here only DX/DY change
// per point, so each PSET is a 2-field update + one register upload rather than a
// full 7-field DrawCmd rebuild through Draw_Point. Same 8 points, same order.
static void PlotOctants(DrawCmd* cmd, UX x, UY y, u16 ox, u16 oy, u8* primed)
{
	// The first point of the whole circle uploads all seven fields; every later one
	// only needs DX/DY, since NX/NY/CLR/ARG/CMD remain latched in the VDP.
	cmd->DX = x + ox; cmd->DY = y + oy;
	if (*primed)
	{
		SendPsetBegin();
		SendPsetPoint(cmd);
	}
	else
	{
		SendCmd36(cmd);          // does its own di/ei, leaves R#15 == 0
		*primed = 1;
		SendPsetBegin();
	}
	cmd->DX = x - ox;                   SendPsetPoint(cmd);
	cmd->DX = x + ox; cmd->DY = y - oy; SendPsetPoint(cmd);
	cmd->DX = x - ox;                   SendPsetPoint(cmd);
	cmd->DX = x + oy; cmd->DY = y + ox; SendPsetPoint(cmd);
	cmd->DX = x - oy;                   SendPsetPoint(cmd);
	cmd->DX = x + oy; cmd->DY = y - ox; SendPsetPoint(cmd);
	cmd->DX = x - oy;                   SendPsetPoint(cmd);
	SendPsetEnd();
}

void Draw_Circle(UX x, UY y, u8 radius, u8 color, u8 op)
{
	DrawCmd cmd;
	i16 f = 1 - (i16)radius;
	i16 ddF_x = 1;
	i16 ddF_y = -2 * (i16)radius;
	u16 ox = 0;
	u16 oy = radius;

	cmd.NX  = 1;
	cmd.NY  = 1;
	cmd.CLR = color;
	cmd.ARG = 0;
	cmd.CMD = VDP_CMD_PSET | op;

	u8 primed = 0;
	PlotOctants(&cmd, x, y, ox, oy, &primed);
	while (ox < oy)
	{
		if (f >= 0)
		{
			oy--;
			ddF_y += 2;
			f += ddF_y;
		}
		ox++;
		ddF_x += 2;
		f += ddF_x;
		PlotOctants(&cmd, x, y, ox, oy, &primed);
	}
}
