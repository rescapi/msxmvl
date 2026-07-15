// msxmvl - clean-room reimplementation of MSXgl "scroll" module (bodies).
// Signatures + documented behaviour from MSXgl engine/src/scroll.h and the
// documented scroll config macros only. No MSXgl implementation source read.
//
// Model
// -----
// The module scrolls a viewport over a source tilemap held in RAM at
// g_Scroll_Map (row stride = SCROLL_SRC_W bytes, SCROLL_SRC_H rows). The visible
// window (SCROLL_DST_W x SCROLL_DST_H tiles) is rendered into the VDP layout
// (name) table at tile position (SCROLL_DST_X, SCROLL_DST_Y), where the name
// table is SCROLL_SCREEN_W tiles wide and based at the VDP layout table in VRAM.
//
//   g_Scroll_OffsetX/Y : viewport origin in PIXELS
//   g_Scroll_TileX/Y   : viewport origin in TILES (= pixel offset >> 3)
//
// Whole-tile movement is realised by (re)drawing the window from the source map;
// the sub-tile (0..7 px) remainder is realised through the VDP display-adjust
// register R#18 (horizontal) and the vertical scroll register R#23 (vertical).
// When SCROLL_WRAP is set the viewport wraps around the source map borders.
//
// Build:  sdcc -c -mz80 --sdcccall 1 scroll.c

// vdp.h first (before scroll.h, so scroll.h's #ifndef fallbacks stay quiet). It
// supplies the runtime table bases and the sprite helpers the mask setup needs.
// MSXgl's scroll.h pulls vdp.h itself (#pragma once), so this is a no-op there.
#include "vdp.h"
#include "scroll.h"
// The demo config maps SCROLL_MASK_COLOR to COLOR_BLACK (MSXgl's color.h). That
// header isn't in this module's include set, so provide the same value (1) as a
// guarded fallback; if color.h is already in the chain its define wins.
#ifndef COLOR_BLACK
	#define COLOR_BLACK		1
#endif

// The name table and sprite attribute table live wherever the program pointed the
// VDP at them, so read the runtime bases that vdp.c tracks -- exactly as MSXgl does.
// (Hardcoding them, as an earlier SCROLL_NT_ADDR/SCROLL_ATTR_ADDR constant did, wrote
// the tilemap to a fixed 0x1800 and silently drew nothing when the layout table sat
// elsewhere -- s_scroll puts it at 0x3800.) Declared here because msxmvl's own
// scroll.h does not pull in vdp.h; a duplicate extern is harmless when it does.


//=============================================================================
// GLOBALS
//=============================================================================

u16 g_Scroll_Map;

#if (SCROLL_HORIZONTAL)
u16 g_Scroll_OffsetX;
u8  g_Scroll_TileX;
#endif

#if (SCROLL_VERTICAL)
u16 g_Scroll_OffsetY;
u8  g_Scroll_TileY;
#endif

#if (SCROLL_ADJUST)
u8  g_Scroll_Adjust;
#endif

//=============================================================================
// LOW-LEVEL VDP HELPERS (MSX I/O ports 0x98 VRAM data, 0x99 register/address)
//=============================================================================

// Set VDP control register <reg> to <val>.  reg -> A, val -> E (--sdcccall 1).
static void VDP_SetReg(u8 reg, u8 val) __naked
{
	reg; val;
	__asm
		; SDCC --sdcccall 1: (u8 reg, u8 val) -> reg in A, val in L (NOT E).
		ld   c, a          ; save register number (A)
		ld   a, l          ; value (L) first
		out  (0x99), a
		ld   a, c
		or   a, #0x80      ; register-write command
		out  (0x99), a
		ret
	__endasm;
}

// Set the VRAM auto-increment write address.  addr -> HL (--sdcccall 1).
// Also programs R#14 (address bits 16-14) so addresses above 16 KB work.
// ISR safety: this is two consecutive 2-write pairs on port 0x99. The MSX interrupt
// handler reads the VDP status port, and a status read RESETS the VDP's address/data
// flip-flop -- so an interrupt landing between the halves of either pair leaves the VDP
// treating our second byte as a first one. The VRAM write address then points somewhere
// arbitrary and the tilemap is written into the void. Keep both pairs atomic.
static void VDP_SetWriteAddr(u16 addr) __naked
{
	addr;
	__asm
		ld   a, h          ; high byte of address
		rlca
		rlca
		and  a, #0x03      ; bits 15-14 -> b1,b0  (bit16 = 0)
		di
		out  (0x99), a
		ld   a, #0x8e      ; R#14 (0x0E | 0x80)
		out  (0x99), a
		ld   a, l          ; address low
		out  (0x99), a
		ld   a, h
		and  a, #0x3f
		or   a, #0x40      ; write-enable bit
		ei                 ; EI has a one-instruction delay, covering the OUT below
		out  (0x99), a
		ret
	__endasm;
}

// Write one byte to VRAM at the current auto-increment address.  val -> A.
static void VDP_WriteByte(u8 val) __naked
{
	val;
	__asm
		out  (0x98), a
		ret
	__endasm;
}

// Block-copy <count> bytes from RAM <src> to VRAM at the current auto-increment
// write address (set beforehand via VDP_SetWriteAddr).
//
// ABI (SDCC --sdcccall 1): a 16-bit first argument goes in HL, but an 8-bit SECOND
// argument does NOT land in a register -- only a 16-bit arg2 uses DE -- so `count`
// arrives on the STACK, and callee-cleanup means we must drop it before returning.
// This routine used to read count from A (garbage) and leave the byte on the stack,
// so it blitted a random run length and leaked a byte of stack on every call.
static void Scroll_BlitRun(const u8* src, u8 count) __naked
{
	src; count;
	__asm
		push hl            ; save src
		ld   hl, #4        ; saved_src(2) + ret(2) -> count
		add  hl, sp
		ld   a, (hl)       ; A = count
		pop  hl            ; HL = src
		or   a, a          ; count == 0 -> nothing to do (otir with B=0 = 256!)
		jr   z, 1$
		ld   b, a          ; B = count
		ld   c, #0x98      ; VRAM data port
		otir               ; OUT (0x98) <- (HL++); B--; repeat until B==0
	1$:
		pop  hl            ; HL = return address
		inc  sp            ; discard the 1-byte stack arg (callee cleanup)
		jp   (hl)
	__endasm;
}

//=============================================================================
// INTERNAL RENDERING
//=============================================================================

// Render the whole visible window from the source map into the name table.
// Each destination row is a contiguous run of source bytes (the source map row
// is linear), so it is uploaded as one - or, when the horizontal viewport wraps
// the source-map right edge, two - block OUTI copies instead of a per-tile loop.
// Byte-for-byte identical output to the former per-tile Scroll_SrcAddr version.
static void Scroll_DrawWindow(void)
{
	u16 row;

	for (row = 0; row < (u16)SCROLL_DST_H; row++)
	{
		u16 nt = g_ScreenLayoutLow
		       + ((u16)SCROLL_DST_Y + row) * (u16)SCROLL_SCREEN_W
		       + (u16)SCROLL_DST_X;
		u16 sy, rowbase, sx;

#if (SCROLL_VERTICAL)
		sy = (u16)SCROLL_SRC_Y + (u16)g_Scroll_TileY + row;
#else
		sy = (u16)SCROLL_SRC_Y + row;
#endif
#if (SCROLL_WRAP)
		while (sy >= (u16)SCROLL_SRC_H) sy -= (u16)SCROLL_SRC_H;
#endif
		rowbase = g_Scroll_Map + sy * (u16)SCROLL_SRC_W;

		// NOTE: the horizontal origin is the scroll tile offset ALONE. MSXgl's
		// Scroll_Update honours SCROLL_SRC_Y but never SCROLL_SRC_X, so adding
		// SCROLL_SRC_X here shifted the whole viewport sideways (by 64 tiles in
		// the s_scroll sample) relative to a real MSXgl build. Match MSXgl.
#if (SCROLL_HORIZONTAL)
		sx = (u16)g_Scroll_TileX;
#else
		sx = 0;
#endif
#if (SCROLL_WRAP)
		while (sx >= (u16)SCROLL_SRC_W) sx -= (u16)SCROLL_SRC_W;
#endif

		VDP_SetWriteAddr(nt);
#if (SCROLL_WRAP)
		if (sx + (u16)SCROLL_DST_W > (u16)SCROLL_SRC_W)
		{
			u8 sw = (u8)((u16)SCROLL_SRC_W - sx);           // bytes up to wrap
			Scroll_BlitRun((const u8*)(rowbase + sx), sw);
			Scroll_BlitRun((const u8*)rowbase, (u8)((u16)SCROLL_DST_W - sw));
		}
		else
#endif
			Scroll_BlitRun((const u8*)(rowbase + sx), (u8)SCROLL_DST_W);
	}
}

// Apply the sub-tile (0..7 px) remainder through the VDP fine-scroll registers.
static void Scroll_ApplyAdjust(void)
{
#if (SCROLL_ADJUST)
	// Match MSXgl exactly: the sub-tile remainder is the R#18 display-adjust register,
	// low nibble = horizontal, high nibble = vertical (both raw 0..7, NOT negated, and
	// NOT R#23 -- R#23 would shift the whole screen including any status rows above the
	// split). g_Scroll_Adjust caches it for the screen-split ISR.
	u8 adj = 0;
  #if (SCROLL_HORIZONTAL)
	adj |= (u8)(g_Scroll_OffsetX & 7);
  #endif
  #if (SCROLL_VERTICAL)
	adj |= (u8)((g_Scroll_OffsetY & 7) << 4);
  #endif
	g_Scroll_Adjust = adj;
  #if (!SCROLL_ADJUST_SPLIT)
	// No split: apply the adjust to the whole display now. With the split enabled
	// (default) R#18 is instead driven per-frame by Scroll_HBlankAdjust so the fine
	// scroll is confined below the split line, leaving the top DST_Y rows steady.
	VDP_SetReg(18, adj);
  #endif
#endif
}

//=============================================================================
// PUBLIC API
//=============================================================================

// Scroll_Initialize: bind the source map, reset offsets, render the initial
// window and return the first sprite index left free for the caller.
u8 Scroll_Initialize(u16 map)
{
	g_Scroll_Map = map;

	// Tile caches are render-invalidation markers: seed to 0xFF so the first
	// Scroll_Update forces a full redraw. The initial window draw is deferred to
	// that first Update (as MSXgl does) rather than drawn eagerly here - drawing
	// at tile (0,0) now only to redraw the same window on the first Update was
	// pure duplicated work. The realistic Initialize-then-Update sequence yields
	// byte-identical VRAM and fine-scroll registers to the former eager draw.
#if (SCROLL_HORIZONTAL)
	g_Scroll_OffsetX = 0;
	g_Scroll_TileX   = 0xFF;
#endif
#if (SCROLL_VERTICAL)
	g_Scroll_OffsetY = 0;
	g_Scroll_TileY   = 0xFF;
#endif
#if (SCROLL_ADJUST)
	g_Scroll_Adjust  = 0;
#endif

	u8 sprtId = SCROLL_MASK_ID;

#if ((SCROLL_HORIZONTAL) && (SCROLL_MASK))
	// Mask sprites hide the seam at the left edge while the window scrolls. Mirrors
	// MSXgl: enable sprites, use 16x16 MAGNIFIED patterns (32 px tall), fill the mask
	// pattern solid, then lay down TWO columns of sprites -- one drawn with the early
	// clock bit (shifted 32 px left) and one without -- and park the rest off-screen.
	// The previous code wrote raw 4-byte sprite-mode-1 attributes, never enabled
	// sprites, never set the sprite flags and never uploaded the mask pattern.
	{
		u8 i;
		u8 y;
		VDP_EnableSprite(TRUE);
		VDP_SetSpriteFlag(VDP_SPRITE_SIZE_16 | VDP_SPRITE_SCALE_2);
		VDP_FillVRAM_16K(0xFF, (u16)(g_SpritePatternLow + (u16)SCROLL_MASK_PATTERN * 4), 8 * 4);

		y = (u8)((SCROLL_DST_Y) * 8 - 1);
		for (i = 0; i < ((SCROLL_DST_H) + 3) / 4; ++i)
		{
			VDP_SetSpriteExUniColor(sprtId++, 0, y, 0, (u8)(SCROLL_MASK_COLOR + VDP_SPRITE_EC));
			y += 32;
		}
		y = (u8)((SCROLL_DST_Y) * 8 - 1);
		for (i = 0; i < ((SCROLL_DST_H) + 3) / 4; ++i)
		{
			VDP_SetSpriteExUniColor(sprtId++, 0, y, 0, SCROLL_MASK_COLOR);
			y += 32;
		}
		VDP_DisableSpritesFrom(sprtId);
	}
#endif
	return sprtId;
}

#if (SCROLL_HORIZONTAL)
// Scroll_SetOffsetH: advance the horizontal viewport origin by <offset> pixels.
void Scroll_SetOffsetH(i8 offset)
{
	i16 v = (i16)g_Scroll_OffsetX + (i16)offset;

#if (SCROLL_WRAP)
	{
		i16 w = (i16)((u16)SCROLL_SRC_W * 8);
		while (v < 0)   v += w;
		while (v >= w)  v -= w;
	}
#else
	{
		// Clamp to [0, (SRC_W-DST_W)*8] like MSXgl. Only clamping the low end (as this
		// used to) let a right scroll run OffsetX past the map's right edge, so
		// Scroll_DrawWindow blitted source bytes beyond the map in a non-wrap build.
		i16 max = (i16)((u16)((u16)(SCROLL_SRC_W - SCROLL_DST_W) * 8));
		if (v < 0)   v = 0;
		if (v > max) v = max;
	}
#endif

	g_Scroll_OffsetX = (u16)v;
	// TileX is a render cache updated in Scroll_Update, not here.
}
#endif

#if (SCROLL_VERTICAL)
// Scroll_SetOffsetV: advance the vertical viewport origin by <offset> pixels.
void Scroll_SetOffsetV(i8 offset)
{
	// Vertical offset is CLAMPED to the non-visible vertical margin of the source
	// map, [0, (SCROLL_SRC_H - SCROLL_DST_H) * 8] pixels (it does not wrap like
	// the horizontal axis). TileY is a render cache updated in Scroll_Update.
	i16 v   = (i16)g_Scroll_OffsetY + (i16)offset;
	i16 max = (i16)(((u16)SCROLL_SRC_H - (u16)SCROLL_DST_H) * 8);

	if (v < 0)   v = 0;
	if (v > max) v = max;

	g_Scroll_OffsetY = (u16)v;
}
#endif

#if ((SCROLL_ADJUST) && (SCROLL_ADJUST_SPLIT))
// Screen-split line-interrupt lines (R#19) that drive the H-Blank state machine.
// Derived by black-box observation of MSXgl across several DST_Y/DST_H configs:
//   frame-start split  = SCROLL_DST_Y*8 - 7          (top of the scroll window)
//   scroll-region split= (SCROLL_DST_Y+SCROLL_DST_H)*8 - 4  (bottom of window)
#define SCROLL_SPLIT_LINE_TOP    ((u8)((u16)(SCROLL_DST_Y) * 8 - 7))
#define SCROLL_SPLIT_LINE_BOTTOM ((u8)(((u16)(SCROLL_DST_Y) + (u16)(SCROLL_DST_H)) * 8 - 4))

// Read VDP status register S#2 (select via R#15, read port 0x99, restore R#15=0).
// MSXgl performs this status-sync inside HBlankAdjust before touching R#18; it
// leaves R#15 pointing back at S#0. The read value is discarded.
static void Scroll_StatusSync(void) __naked
{
	__asm
		ld   a, #0x02
		out  (0x99), a
		ld   a, #0x8f      ; select S#2 (R#15 = 2)
		out  (0x99), a
		in   a, (0x99)     ; read S#2 (discarded)
		xor  a, a
		out  (0x99), a
		ld   a, #0x8f      ; restore S#0 (R#15 = 0)
		out  (0x99), a
		ret
	__endasm;
}

// Scroll_HBlankAdjust: drive the display screen-split state machine from the
// H-Blank (line-interrupt) handler.
//   0: frame start   -> arm the split at the top of the scroll window (R#19)
//   1: scrolling adj -> apply the global sub-tile adjust (R#18 = g_Scroll_Adjust)
//                       and arm the split at the bottom of the window (R#19)
//   2: no more adjust-> clear the display adjust (R#18 = 0)
void Scroll_HBlankAdjust(u8 adjust)
{
	if (adjust == 0)          // frame start
	{
		VDP_SetReg(19, SCROLL_SPLIT_LINE_TOP);
	}
	else if (adjust == 1)     // scrolling region
	{
		Scroll_StatusSync();
		VDP_SetReg(18, g_Scroll_Adjust);
		VDP_SetReg(19, SCROLL_SPLIT_LINE_BOTTOM);
	}
	else                      // adjust == 2: no more adjust
	{
		Scroll_StatusSync();
		VDP_SetReg(18, 0);
	}
}
#endif

// Scroll_Update: apply the fine (sub-tile) adjust every call, and redraw the
// window only when the whole-tile viewport position actually changed. Redrawing
// the same tile position writes identical bytes, so skipping it when unchanged
// (the common case - most frames move <8px) is byte-for-byte equivalent while
// avoiding a full-window VRAM rewrite on every call. g_Scroll_TileX/Y hold the
// last DRAWN tile position (Initialize seeds them to 0xFF => first Update draws).
void Scroll_Update(void)
{
#if (SCROLL_HORIZONTAL)
	u8 newTileX = (u8)((g_Scroll_OffsetX >> 3) & 0xFF);
#endif
#if (SCROLL_VERTICAL)
	u8 newTileY = (u8)((g_Scroll_OffsetY >> 3) & 0xFF);
#endif

	Scroll_ApplyAdjust();

	// Reposition the seam-mask sprites EVERY frame -- before the tile-change early
	// return, as MSXgl does. One column tracks the left edge of the window at the
	// sub-tile pixel remainder, the other the right edge (DST_W-1 tiles further on).
	// msxmvl used to place these once in Scroll_Initialize and never move them, so
	// the right-hand column stayed at x=0 instead of following the window.
#if ((SCROLL_HORIZONTAL) && (SCROLL_MASK))
	{
		u8 sprtId = SCROLL_MASK_ID;
		u8 stepX  = (u8)(g_Scroll_OffsetX & 0x07);
		u8 i;
		stepX += (u8)((SCROLL_DST_X) * 8);
		for (i = 0; i < ((SCROLL_DST_H) + 3) / 4; ++i)
			VDP_SetSpritePositionX(sprtId++, stepX);
		stepX += (u8)(((SCROLL_DST_W) - 1) * 8);
		for (i = 0; i < ((SCROLL_DST_H) + 3) / 4; ++i)
			VDP_SetSpritePositionX(sprtId++, stepX);
	}
#endif

#if ((SCROLL_HORIZONTAL) && (SCROLL_VERTICAL))
	if ((newTileX == g_Scroll_TileX) && (newTileY == g_Scroll_TileY))
		return;
#elif (SCROLL_HORIZONTAL)
	if (newTileX == g_Scroll_TileX)
		return;
#elif (SCROLL_VERTICAL)
	if (newTileY == g_Scroll_TileY)
		return;
#endif

#if (SCROLL_HORIZONTAL)
	g_Scroll_TileX = newTileX;
#endif
#if (SCROLL_VERTICAL)
	g_Scroll_TileY = newTileY;
#endif

	Scroll_DrawWindow();
}
