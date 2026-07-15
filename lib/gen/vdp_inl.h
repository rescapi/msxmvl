// msxmvl clean-room reimplementation of MSXgl "vdp_inl" module.
// Public names + signatures match MSXgl exactly (drop-in replacement).
// Derived from MSXgl engine/src/vdp_inl.h (interface only),
// MSXgl engine/src/vdp.h and vdp_reg.h (contracts only) + V9938 knowledge.
//
// In MSXgl these are `inline` functions living in vdp.h's inline section. msxmvl
// exposes them as REAL, linkable functions with the exact same names/signatures
// so a single difftest driver can link against real MSXgl OR msxmvl.
//
// This module only builds the VDP command block (the g_VDP_Command struct) and
// then hands off to the `vdp` module's command-setup helpers. Those helpers and
// the g_VDP_Command global are part of the SEPARATE `vdp` module: declared here
// (the contract) and resolved at link time by whichever library is linked.
#ifndef MSXMVL_VDP_INL_H
#define MSXMVL_VDP_INL_H

#include "types.h"

//=============================================================================
// SDCC attribute macros (mirror MSXgl core.h)
//=============================================================================
#ifndef __FASTCALL
#define __FASTCALL		__z88dk_fastcall
#endif
#ifndef __NAKED
#define __NAKED			__naked
#endif
#ifndef __PRESERVES
#define __PRESERVES		__preserves_regs
#endif

//=============================================================================
// VDP COMMAND CONSTANTS (from vdp_reg.h)
//=============================================================================
// Command opcodes (high nibble of R46)
#define VDP_CMD_HMMC		0xF0
#define VDP_CMD_YMMM		0xE0
#define VDP_CMD_HMMM		0xD0
#define VDP_CMD_HMMV		0xC0
#define VDP_CMD_LMMC		0xB0
#define VDP_CMD_LMCM		0xA0
#define VDP_CMD_LMMM		0x90
#define VDP_CMD_LMMV		0x80
#define VDP_CMD_LINE		0x70
#define VDP_CMD_SRCH		0x60
#define VDP_CMD_PSET		0x50
#define VDP_CMD_POINT		0x40
#define VDP_CMD_STOP		0x00

// Logical operations (low nibble of R46)
#define VDP_OP_IMP			0x00
#define VDP_OP_AND			0x01
#define VDP_OP_OR			0x02
#define VDP_OP_XOR			0x03
#define VDP_OP_NOT			0x04
#define VDP_OP_TIMP			0x08
#define VDP_OP_TAND			0x09
#define VDP_OP_TOR			0x0A
#define VDP_OP_TXOR			0x0B
#define VDP_OP_TNOT			0x0C

// Argument register (R45) flags
#define VDP_ARG_DIY_DOWN	0
#define VDP_ARG_DIY_UP		8
#define VDP_ARG_DIX_RIGHT	0
#define VDP_ARG_DIX_LEFT	4
#define VDP_ARG_MAJ_H		0
#define VDP_ARG_MAJ_V		1

//=============================================================================
// VDP COMMAND BUFFER (owned by the `vdp` module)
//=============================================================================
// Register data for a VDP command (maps to R32..R46). 15 bytes.
typedef struct VDP_Command
{
	u16 SX;  // 32-33
	u16 SY;  // 34-35
	u16 DX;  // 36-37
	u16 DY;  // 38-39
	u16 NX;  // 40-41
	u16 NY;  // 42-43
	u8  CLR; // 44
	u8  ARG; // 45
	u8  CMD; // 46
} VDP_Command;
#define VDP_Command32 VDP_Command

// Register data for a VDP command starting at R36. 11 bytes.
typedef struct VDP_Command36
{
	u16 DX;  // 36-37
	u16 DY;  // 38-39
	u16 NX;  // 40-41
	u16 NY;  // 42-43
	u8  CLR; // 44
	u8  ARG; // 45
	u8  CMD; // 46
} VDP_Command36;

// VDP command buffer structure (defined by the `vdp` module).
extern VDP_Command g_VDP_Command;

//=============================================================================
// `vdp` module helpers used by these routines (contract only; linked externally)
//=============================================================================
// Write VDP register (indirect). [MSX1/2/2+/TR]
void VDP_RegWrite(u8 reg, u8 value) __PRESERVES(b, c, d, e, iyl, iyh);
// Read a VDP status register. [MSX2/2+/TR]
u8   VDP_ReadStatus(u8 stat) __PRESERVES(b, c, d, e, h, iyl, iyh);
// Wait for the previous VDP command to finish. [MSX2/2+/TR]
void VDP_CommandWait() __PRESERVES(b, c, d, e, h, l, iyl, iyh);
// Send VDP command from g_VDP_Command (registers 32 to 46). [MSX2/2+/TR]
void VDP_CommandSetupR32();
// Send VDP command from g_VDP_Command (registers 36 to 46). [MSX2/2+/TR]
void VDP_CommandSetupR36();
// CPU->VRAM write command loop. [MSX2/2+/TR]
void VDP_CommandWriteLoop(const u8* addr) __FASTCALL __PRESERVES(d, e, iyl, iyh);

//=============================================================================
// VDP COMMANDS (this module)
//=============================================================================
// The header declares no __PRESERVES() contract on any of these (they are
// `inline` in MSXgl), so they follow the standard SDCC calling convention.

// High speed move CPU to VRAM, with explicit argument register. [MSX2/2+/TR]
void VDP_CommandHMMC_Arg(const u8* addr, u16 dx, u16 dy, u16 nx, u16 ny, u8 arg);
// High speed move CPU to VRAM. [MSX2/2+/TR]
void VDP_CommandHMMC(const u8* addr, u16 dx, u16 dy, u16 nx, u16 ny);
// High speed move VRAM to VRAM, Y coordinate only. [MSX2/2+/TR]
void VDP_CommandYMMM(u16 sy, u16 dx, u16 dy, u16 ny, u8 dir);
// High speed move VRAM to VRAM, with explicit argument register. [MSX2/2+/TR]
void VDP_CommandHMMM_Arg(u16 sx, u16 sy, u16 dx, u16 dy, u16 nx, u16 ny, u8 arg);
// High speed move VRAM to VRAM. [MSX2/2+/TR]
void VDP_CommandHMMM(u16 sx, u16 sy, u16 dx, u16 dy, u16 nx, u16 ny);
// High speed move VDP to VRAM (fill), with explicit argument register. [MSX2/2+/TR]
void VDP_CommandHMMV_Arg(u16 dx, u16 dy, u16 nx, u16 ny, u8 col, u8 arg);
// High speed move VDP to VRAM (fill). [MSX2/2+/TR]
void VDP_CommandHMMV(u16 dx, u16 dy, u16 nx, u16 ny, u8 col);
// Logical move CPU to VRAM, with explicit argument register. [MSX2/2+/TR]
void VDP_CommandLMMC_Arg(const u8* addr, u16 dx, u16 dy, u16 nx, u16 ny, u8 op, u8 arg);
// Logical move CPU to VRAM. [MSX2/2+/TR]
void VDP_CommandLMMC(const u8* addr, u16 dx, u16 dy, u16 nx, u16 ny, u8 op);
// Logical move VRAM to CPU (not implemented). [MSX2/2+/TR]
void VDP_CommandLMCM();
// Logical move VRAM to VRAM, with explicit argument register. [MSX2/2+/TR]
void VDP_CommandLMMM_Arg(u16 sx, u16 sy, u16 dx, u16 dy, u16 nx, u16 ny, u8 op, u8 arg);
// Logical move VRAM to VRAM. [MSX2/2+/TR]
void VDP_CommandLMMM(u16 sx, u16 sy, u16 dx, u16 dy, u16 nx, u16 ny, u8 op);
// Logical move VDP to VRAM (fill), with explicit argument register. [MSX2/2+/TR]
void VDP_CommandLMMV_Arg(u16 dx, u16 dy, u16 nx, u16 ny, u8 col, u8 op, u8 arg);
// Logical move VDP to VRAM (fill). [MSX2/2+/TR]
void VDP_CommandLMMV(u16 dx, u16 dy, u16 nx, u16 ny, u8 col, u8 op);
// Draw a straight line in VRAM. [MSX2/2+/TR]
void VDP_CommandLINE(u16 dx, u16 dy, u16 nx, u16 ny, u8 col, u8 arg, u8 op);
// Search for a color in VRAM left/right of the start point. [MSX2/2+/TR]
void VDP_CommandSRCH(u16 sx, u16 sy, u8 col, u8 arg);
// Draw a dot in VRAM. [MSX2/2+/TR]
void VDP_CommandPSET(u16 dx, u16 dy, u8 col, u8 op);
// Read the color of a dot in VRAM. [MSX2/2+/TR]
u8   VDP_CommandPOINT(u16 sx, u16 sy);
// Abort the current command. [MSX2/2+/TR]
void VDP_CommandSTOP();

//=============================================================================
// ALIASES (from vdp.h)
//=============================================================================
#define VDP_CopyRAMtoVRAM			VDP_CommandHMMC
#define VDP_YMoveVRAM				VDP_CommandYMMM
#define VDP_MoveVRAM				VDP_CommandHMMM
#define VDP_LogicalCopyRAMtoVRAM	VDP_CommandLMMC
#define VDP_LogicalYMoveVRAM		VDP_CommandLMCM
#define VDP_LogicalMoveVRAM			VDP_CommandLMMM
#define VDP_LogicalFillVRAM			VDP_CommandLMMV
#define VDP_DrawLine				VDP_CommandLINE
#define VDP_SearchColor				VDP_CommandSRCH
#define VDP_DrawPoint				VDP_CommandPSET
#define VDP_ReadPoint				VDP_CommandPOINT
#define VDP_AbortCommand			VDP_CommandSTOP

#endif // MSXMVL_VDP_INL_H
