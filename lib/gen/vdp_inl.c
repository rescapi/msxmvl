// msxmvl clean-room reimplementation of MSXgl "vdp_inl" module.
// Toolchain: SDCC --sdcccall 1.
//
// Each routine fills the g_VDP_Command block (which maps to VDP registers
// R32..R46) and then triggers the transfer via the `vdp` module's setup
// helpers. Behaviour mirrors the documented MSXgl inline bodies exactly.
//
// The header declares NO __PRESERVES() contract for any of these (they are
// `inline` in MSXgl), so they use the standard SDCC convention. Written in
// plain C so SDCC generates the field stores / helper calls; the register
// footprint of an inline expansion is not part of the contract.

#include "vdp_inl.h"

//=============================================================================
// CPU -> VRAM
//=============================================================================

// High speed move CPU to VRAM (R36-based; sources first byte, then loops).
void VDP_CommandHMMC_Arg(const u8* addr, u16 dx, u16 dy, u16 nx, u16 ny, u8 arg)
{
	g_VDP_Command.DX = dx;
	g_VDP_Command.DY = dy;
	g_VDP_Command.NX = nx;
	g_VDP_Command.NY = ny;
	g_VDP_Command.CLR = *addr;
	g_VDP_Command.ARG = arg;
	g_VDP_Command.CMD = VDP_CMD_HMMC;
	VDP_CommandSetupR36();
	VDP_CommandWriteLoop(addr);
}

void VDP_CommandHMMC(const u8* addr, u16 dx, u16 dy, u16 nx, u16 ny)
{
	VDP_CommandHMMC_Arg(addr, dx, dy, nx, ny, 0);
}

//=============================================================================
// VRAM -> VRAM
//=============================================================================

// High speed move VRAM to VRAM, Y coordinate only (R32-based).
void VDP_CommandYMMM(u16 sy, u16 dx, u16 dy, u16 ny, u8 dir)
{
	g_VDP_Command.SY = sy;
	g_VDP_Command.DX = dx;
	g_VDP_Command.DY = dy;
	g_VDP_Command.NY = ny;
	g_VDP_Command.ARG = dir;
	g_VDP_Command.CMD = VDP_CMD_YMMM;
	VDP_CommandSetupR32();
}

// High speed move VRAM to VRAM (R32-based).
void VDP_CommandHMMM_Arg(u16 sx, u16 sy, u16 dx, u16 dy, u16 nx, u16 ny, u8 arg)
{
	g_VDP_Command.SX = sx;
	g_VDP_Command.SY = sy;
	g_VDP_Command.DX = dx;
	g_VDP_Command.DY = dy;
	g_VDP_Command.NX = nx;
	g_VDP_Command.NY = ny;
	g_VDP_Command.ARG = arg;
	g_VDP_Command.CMD = VDP_CMD_HMMM;
	VDP_CommandSetupR32();
}

void VDP_CommandHMMM(u16 sx, u16 sy, u16 dx, u16 dy, u16 nx, u16 ny)
{
	VDP_CommandHMMM_Arg(sx, sy, dx, dy, nx, ny, 0);
}

//=============================================================================
// VDP -> VRAM (fill)
//=============================================================================

// High speed fill (R36-based).
void VDP_CommandHMMV_Arg(u16 dx, u16 dy, u16 nx, u16 ny, u8 col, u8 arg)
{
	g_VDP_Command.DX = dx;
	g_VDP_Command.DY = dy;
	g_VDP_Command.NX = nx;
	g_VDP_Command.NY = ny;
	g_VDP_Command.CLR = col;
	g_VDP_Command.ARG = arg;
	g_VDP_Command.CMD = VDP_CMD_HMMV;
	VDP_CommandSetupR36();
}

void VDP_CommandHMMV(u16 dx, u16 dy, u16 nx, u16 ny, u8 col)
{
	VDP_CommandHMMV_Arg(dx, dy, nx, ny, col, 0);
}

//=============================================================================
// LOGICAL CPU -> VRAM
//=============================================================================

void VDP_CommandLMMC_Arg(const u8* addr, u16 dx, u16 dy, u16 nx, u16 ny, u8 op, u8 arg)
{
	g_VDP_Command.DX = dx;
	g_VDP_Command.DY = dy;
	g_VDP_Command.NX = nx;
	g_VDP_Command.NY = ny;
	g_VDP_Command.CLR = *addr;
	g_VDP_Command.ARG = arg;
	g_VDP_Command.CMD = VDP_CMD_LMMC + op;
	VDP_CommandSetupR36();
	VDP_CommandWriteLoop(addr);
}

void VDP_CommandLMMC(const u8* addr, u16 dx, u16 dy, u16 nx, u16 ny, u8 op)
{
	VDP_CommandLMMC_Arg(addr, dx, dy, nx, ny, op, 0);
}

//=============================================================================
// LOGICAL VRAM -> CPU (not implemented in MSXgl)
//=============================================================================

void VDP_CommandLMCM()
{
}

//=============================================================================
// LOGICAL VRAM -> VRAM
//=============================================================================

void VDP_CommandLMMM_Arg(u16 sx, u16 sy, u16 dx, u16 dy, u16 nx, u16 ny, u8 op, u8 arg)
{
	g_VDP_Command.SX = sx;
	g_VDP_Command.SY = sy;
	g_VDP_Command.DX = dx;
	g_VDP_Command.DY = dy;
	g_VDP_Command.NX = nx;
	g_VDP_Command.NY = ny;
	g_VDP_Command.ARG = arg;
	g_VDP_Command.CMD = VDP_CMD_LMMM + op;
	VDP_CommandSetupR32();
}

void VDP_CommandLMMM(u16 sx, u16 sy, u16 dx, u16 dy, u16 nx, u16 ny, u8 op)
{
	VDP_CommandLMMM_Arg(sx, sy, dx, dy, nx, ny, op, 0);
}

//=============================================================================
// LOGICAL VDP -> VRAM (fill)
//=============================================================================

void VDP_CommandLMMV_Arg(u16 dx, u16 dy, u16 nx, u16 ny, u8 col, u8 op, u8 arg)
{
	g_VDP_Command.DX = dx;
	g_VDP_Command.DY = dy;
	g_VDP_Command.NX = nx;
	g_VDP_Command.NY = ny;
	g_VDP_Command.CLR = col;
	g_VDP_Command.ARG = arg;
	g_VDP_Command.CMD = VDP_CMD_LMMV + op;
	VDP_CommandSetupR36();
}

void VDP_CommandLMMV(u16 dx, u16 dy, u16 nx, u16 ny, u8 col, u8 op)
{
	VDP_CommandLMMV_Arg(dx, dy, nx, ny, col, op, 0);
}

//=============================================================================
// PRIMITIVES
//=============================================================================

// Draw a straight line (R36-based).
void VDP_CommandLINE(u16 dx, u16 dy, u16 nx, u16 ny, u8 col, u8 arg, u8 op)
{
	g_VDP_Command.DX = dx;
	g_VDP_Command.DY = dy;
	g_VDP_Command.NX = nx;
	g_VDP_Command.NY = ny;
	g_VDP_Command.CLR = col;
	g_VDP_Command.ARG = arg;
	g_VDP_Command.CMD = VDP_CMD_LINE + op;
	VDP_CommandSetupR36();
}

// Search for a color left/right of the start point (R32-based).
void VDP_CommandSRCH(u16 sx, u16 sy, u8 col, u8 arg)
{
	g_VDP_Command.SX = sx;
	g_VDP_Command.SY = sy;
	g_VDP_Command.CLR = col;
	g_VDP_Command.ARG = arg;
	g_VDP_Command.CMD = VDP_CMD_SRCH;
	VDP_CommandSetupR32();
}

// Draw a dot (R36-based). ARG is forced to 0.
void VDP_CommandPSET(u16 dx, u16 dy, u8 col, u8 op)
{
	g_VDP_Command.DX = dx;
	g_VDP_Command.DY = dy;
	g_VDP_Command.CLR = col;
	g_VDP_Command.ARG = 0;
	g_VDP_Command.CMD = VDP_CMD_PSET + op;
	VDP_CommandSetupR36();
}

// Read the color of a dot (R32-based): issue POINT, wait, read S#7.
u8 VDP_CommandPOINT(u16 sx, u16 sy)
{
	g_VDP_Command.SX = sx;
	g_VDP_Command.SY = sy;
	g_VDP_Command.CMD = VDP_CMD_POINT;
	VDP_CommandSetupR32();
	VDP_CommandWait();
	return VDP_ReadStatus(7);
}

// Abort the current command by writing STOP to R46.
void VDP_CommandSTOP()
{
	VDP_RegWrite(46, VDP_CMD_STOP);
}
