// msxmvl clean-room reimplementation of MSXgl "bios" module.
// Drop-in: exposes MSXgl's exact public names + signatures so a single difftest
// driver can link against real MSXgl OR msxmvl.
//
// Derived ONLY from the interface/contract of MSXgl engine/src/bios.h
// (signatures + doc comments) plus the address/variable definition headers
// (bios_mainrom.h / bios_var.h / vdp_reg.h / ascii.h / input.h) and standard
// MSX/Z80 BIOS knowledge. No MSXgl implementation body was read.
//
// The upstream header declares NO __PRESERVES() contract on any bios function,
// so every routine here follows the plain SDCC --sdcccall 1 convention
// (AF/BC/DE/HL/IX caller-clobberable; IY = SDCC frame pointer is preserved).
#ifndef MSXMVL_BIOS_H
#define MSXMVL_BIOS_H

//=============================================================================
// INCLUDES
//=============================================================================

#include "types.h"

//=============================================================================
// CALL HELPERS (self-contained; mirror system.h / core.h contract)
//=============================================================================
// Direct call a routine at a given address.
inline void Call(u16 addr) { ((void(*)(void))addr)(); }
// Direct call a routine; return register A.
inline u8   CallToA(u16 addr) { return ((u8(*)(void))addr)(); }
// Direct call a routine with a 8-bit parameter in register A.
inline void CallA(u16 addr, u8 a) { ((void(*)(u8))addr)(a); }
// Direct call a routine with a 8-bit parameter in register A; return A.
inline u8   CallAToA(u16 addr, u8 a) { return ((u8(*)(u8))addr)(a); }
// Direct call a routine with a 16-bit parameter in register HL.
inline void CallHL(u16 addr, u16 hl) { ((void(*)(u16))addr)(hl); }
// Direct call a routine with a 16-bit parameter in register HL; return A.
inline u8   CallHLToA(u16 addr, u16 hl) { return ((u8(*)(u16))addr)(hl); }

// Interrupt control.
inline void EnableInterrupt()  { __asm__("ei"); }
inline void DisableInterrupt() { __asm__("di"); }

//=============================================================================
// BUILD CONFIGURATION (upstream: config selects these; MSX2 is the difftest target)
//=============================================================================

#ifndef MSX_1
#define MSX_1			0
#define MSX_2			1
#define MSX_2P			2
#define MSX_TR			3
#endif
#ifndef MSX_VERSION
#define MSX_VERSION		MSX_2
#endif
#ifndef BIOS_USE_VDP
#define BIOS_USE_VDP	1
#endif
#ifndef BIOS_USE_PSG
#define BIOS_USE_PSG	1
#endif

// MSX version number values (RAM at 0x002D)
#define MSXVER_1		0
#define MSXVER_2		1
#define MSXVER_2P		2
#define MSXVER_TR		3

//=============================================================================
// MAIN-ROM ROUTINE ADDRESSES (R_xxxx) - bios_mainrom.h contract
//=============================================================================

#define R_CHKRAM	0x0000
#define R_RDSLT		0x000C
#define R_WRSLT		0x0014
#define R_CALSLT	0x001C
#define R_ENASLT	0x0024
#define R_CALLF		0x0030

#define R_DISSCR	0x0041
#define R_ENASCR	0x0044
#define R_WRTVDP	0x0047
#define R_RDVRM		0x004A
#define R_WRTVRM	0x004D
#define R_SETRD		0x0050
#define R_SETWRT	0x0053
#define R_FILVRM	0x0056
#define R_LDIRMV	0x0059
#define R_LDIRVM	0x005C
#define R_CHGMOD	0x005F
#define R_CHGCLR	0x0062
#define R_CLRSPR	0x0069
#define R_INITXT	0x006C
#define R_INIT32	0x006F
#define R_INIGRP	0x0072
#define R_INIMLT	0x0075
#define R_SETTXT	0x0078
#define R_SETT32	0x007B
#define R_SETGRP	0x007E
#define R_SETMLT	0x0081
#define R_CALPAT	0x0084
#define R_CALATR	0x0087
#define R_GSPSIZ	0x008A
#define R_GRPPRT	0x008D

#define R_GICINI	0x0090
#define R_WRTPSG	0x0093
#define R_RDPSG		0x0096
#define R_STRTMS	0x0099
#define R_CHSNS		0x009C
#define R_CHGET		0x009F
#define R_CHPUT		0x00A2
#define R_LPTOUT	0x00A5
#define R_LPTSTT	0x00A8
#define R_BEEP		0x00C0
#define R_CLS		0x00C3
#define R_POSIT		0x00C6
#define R_TOTEXT	0x00D2
#define R_GTSTCK	0x00D5
#define R_GTTRIG	0x00D8
#define R_GTPAD		0x00DB
#define R_GTPDL		0x00DE
#define R_SNSMAT	0x0141
#define R_OUTDLP	0x014D
#define R_CHGCPU	0x0180
#define R_GETCPU	0x0183

//=============================================================================
// MAIN-ROM DATA + SYSTEM WORK AREA VARIABLES (g_xxxx)
//=============================================================================
// Fixed absolute addresses (bios_mainrom.h / bios_var.h contract). Exposed as
// #define accessors (not `extern __at`) so this module links standalone: an
// `extern __at` reference needs a separate definition, whereas these resolve to
// the fixed hardware/work-area addresses directly. The names + types match the
// MSXgl public interface so driver source is byte-comparable across libraries.

#define g_CGTABL (*(volatile const u16*)0x0004) // ROM: char generator base
#define g_VDP_DR (*(volatile const u8*)0x0006)  // ROM: VDP data read  base port
#define g_VDP_DW (*(volatile const u8*)0x0007)  // ROM: VDP data write base port
#define g_MSXVER (*(volatile const u8*)0x002D)  // ROM: MSX generation

#define g_LINL40 (*(volatile u8*)0xF3AE)
#define g_LINL32 (*(volatile u8*)0xF3AF)
#define g_TXTNAM (*(volatile u16*)0xF3B3)
#define g_TXTCGP (*(volatile u16*)0xF3B7)
#define g_T32NAM (*(volatile u16*)0xF3BD)
#define g_T32COL (*(volatile u16*)0xF3BF)
#define g_T32CGP (*(volatile u16*)0xF3C1)
#define g_T32ATR (*(volatile u16*)0xF3C3)
#define g_T32PAT (*(volatile u16*)0xF3C5)
#define g_GRPNAM (*(volatile u16*)0xF3C7)
#define g_GRPCOL (*(volatile u16*)0xF3C9)
#define g_GRPCGP (*(volatile u16*)0xF3CB)
#define g_GRPATR (*(volatile u16*)0xF3CD)
#define g_GRPPAT (*(volatile u16*)0xF3CF)
u16 __at(0xF3D1) g_MLTNAM;
u16 __at(0xF3D3) g_MLTCOL;
u16 __at(0xF3D5) g_MLTCGP;
u16 __at(0xF3D7) g_MLTATR;
u16 __at(0xF3D9) g_MLTPAT;
#define g_CLIKSW (*(volatile u8*)0xF3DB)
#define g_RG0SAV (*(volatile const u8*)0xF3DF)
#define g_RG1SAV (*(volatile const u8*)0xF3E0)
#define g_RG2SAV (*(volatile const u8*)0xF3E1)
#define g_RG3SAV (*(volatile const u8*)0xF3E2)
#define g_RG4SAV (*(volatile const u8*)0xF3E3)
#define g_RG5SAV (*(volatile const u8*)0xF3E4)
#define g_RG6SAV (*(volatile const u8*)0xF3E5)
#define g_RG7SAV (*(volatile const u8*)0xF3E6)
#define g_STATFL (*(volatile const u8*)0xF3E7)
u8 __at(0xF3E9) g_FORCLR;
u8 __at(0xF3EA) g_BAKCLR;
u8 __at(0xF3EB) g_BDRCLR;
#define g_LOGOPR (*(volatile u8*)0xFB02)
#define g_OLDKEY ((volatile const u8*)0xFBDA)   // 11-byte matrix (previous)
#define g_NEWKEY ((volatile const u8*)0xFBE5)   // 11-byte matrix (current)
#define g_GRPACX (*(volatile u16*)0xFCB7)
#define g_GRPACY (*(volatile u16*)0xFCB9)
#define g_EXPTBL ((volatile const u8*)0xFCC1)   // slot expansion / Main-ROM slot

//=============================================================================
// HELPER MACRO CONSTANTS (vdp_reg.h / ascii.h / input.h contract)
//=============================================================================

#define S00_5S			0x40
#define S00_C			0x20
#define S00_GET_SN(a)	((a) & 0x1F)

#define ASCII_FORM_FEED	12

#define KEY_ROW(key)	((key) & 0x0F)
#define KEY_IDX(key)	((key) >> 4)
#define KEY_FLAG(key)	(1 << KEY_IDX(key))

//=============================================================================
// INPUT MACROS
//=============================================================================

#define KEYBOARD_HOLD(key)    ((g_NEWKEY[KEY_ROW(key)] & KEY_FLAG(key)) == 0)
#define KEYBOARD_PRESS(key)   (((g_NEWKEY[KEY_ROW(key)] & KEY_FLAG(key)) == 0) && ((g_OLDKEY[KEY_ROW(key)] & KEY_FLAG(key)) != 0))
#define KEYBOARD_RELEASE(key) (((g_NEWKEY[KEY_ROW(key)] & KEY_FLAG(key)) != 0) && ((g_OLDKEY[KEY_ROW(key)] & KEY_FLAG(key)) == 0))

//#############################################################################
// Group: Helper
//#############################################################################

// Handle clean exit from Basic or MSX-DOS environment.
void BIOS_Exit(u8 ret);

// Enable or disable key click.
inline void BIOS_SetKeyClick(bool enable) { g_CLIKSW = enable; }

// Get MSX generation version (MSXVER_1..MSXVER_TR).
inline u8 BIOS_GetMSXVersion() { return g_MSXVER; }

//#############################################################################
// Group: Main-ROM - Core
//#############################################################################

// Tests RAM and sets RAM slot for the system. Wrapper for CHKRAM.
inline void BIOS_Reboot() { Call(R_CHKRAM); }

// Reads the value of an address in another slot. Wrapper for RDSLT.
u8 BIOS_InterSlotRead(u8 slot, u16 addr);

// Writes a value to an address in another slot. Wrapper for WRSLT.
void BIOS_InterSlotWrite(u8 slot, u16 addr, u8 value);

// Executes inter-slot call. Wrapper for CALSLT.
void BIOS_InterSlotCall(u8 slot, u16 addr);

// Switches to specified slot and page definitively. Wrapper for ENASLT.
void BIOS_SwitchSlot(u8 page, u8 slot);

// Macro: BIOS_CALLF - Executes an interslot call. Wrapper for CALLF.
// Note: sdasz80 spells define-byte/word as .db/.dw (MSXgl's asm uses db/dw).
#define BIOS_CALLF(slot, addr) \
	__asm                      \
		rst		R_CALLF        \
		.db		slot           \
		.dw		addr           \
	__endasm

//#############################################################################
// Group: Main-ROM - VDP
//#############################################################################
#if (BIOS_USE_VDP)

// Inhibits the screen display. Wrapper for DISSCR.
inline void BIOS_DisableScreen() { Call(R_DISSCR); }

// Displays the screen. Wrapper for ENASCR.
inline void BIOS_EnableScreen() { Call(R_ENASCR); }

// Display or disable the screen.
inline void BIOS_DisplayScreen(bool enable) { if (enable) BIOS_EnableScreen(); else BIOS_DisableScreen(); }

// Writes data in the VDP-register. Wrapper for WRTVDP.
void BIOS_WriteVDP(u8 reg, u8 value);

// Reads data from the VDP-register (from the RAM mirror RG0SAV..RG7SAV, which
// occupy consecutive bytes at 0xF3DF). Valid for reg 0-7 (the mirrored set).
inline u8 BIOS_ReadVDP(u8 reg) { return ((volatile const u8*)0xF3DF)[reg]; }

// Reads the content of VRAM. Wrapper for RDVRM.
inline u8 BIOS_ReadVRAM(u16 addr) { return CallHLToA(R_RDVRM, addr); }

// Writes data in VRAM. Wrapper for WRTVRM.
void BIOS_WriteVRAM(u8 value, u16 addr);

// Enables VDP to read. Wrapper for SETRD.
inline void BIOS_SetAddressForRead(u16 addr) { CallHL(R_SETRD, addr); }

// Enables VDP to write. Wrapper for SETWRT.
inline void BIOS_SetAddressForWrite(u16 addr) { CallHL(R_SETWRT, addr); }

// Fills VRAM with value. Wrapper for FILVRM.
void BIOS_FillVRAM(u16 addr, u16 length, u8 value);

// Block transfer to memory from VRAM. Wrapper for LDIRMV.
void BIOS_CopyFromVRAM(u16 vram, void* ram, u16 length);

// Block transfer to VRAM from memory. Wrapper for LDIRVM.
void BIOS_CopyToVRAM(const void* ram, u16 vram, u16 length);

// Switches to given screen mode. Wrapper for CHGMOD.
inline void BIOS_SetScreenMode(u8 screen) { CallA(R_CHGMOD, screen); }

// Set screen colors (RAM only).
inline void BIOS_SetColor(u8 text, u8 back, u8 border) { g_FORCLR = text; g_BAKCLR = back; g_BDRCLR = border; }

// Changes the screen colors. Wrapper for CHGCLR.
inline void BIOS_ApplyColor(u8 text, u8 back, u8 border) { BIOS_SetColor(text, back, border); Call(R_CHGCLR); }

// Changes the border color. Wrapper for CHGCLR.
inline void BIOS_ApplyBorder(u8 border) { g_BDRCLR = border; Call(R_CHGCLR); }

// Base address of the MSX character set in ROM.
inline u16 BIOS_GetFontAddress() { return g_CGTABL; }

// Base address of a given character in ROM.
inline u16 BIOS_GetCharAddress(u8 chr) { return g_CGTABL + (chr * 8); }

// Base port address for VDP data read.
inline u8 BIOS_GetVDPReadPort() { return g_VDP_DR; }

// Base port address for VDP data write.
inline u8 BIOS_GetVDPWritePort() { return g_VDP_DW; }

//.............................................................................
// Screen 0

inline void BIOS_SetWidth40(u8 width) { g_LINL40 = width; }

// Switches to SCREEN 0. Wrapper for INITXT.
inline void BIOS_InitScreen0() { Call(R_INITXT); }
inline void BIOS_InitScreen0Color(u8 width, u8 text, u8 back, u8 border) { g_LINL40 = width; BIOS_SetColor(text, back, border); BIOS_InitScreen0(); }
inline void BIOS_InitScreen0Ex(u16 pnt, u16 pgt, u8 width, u8 text, u8 back, u8 border) { g_TXTNAM = pnt; g_TXTCGP = pgt; g_LINL40 = width; BIOS_SetColor(text, back, border); BIOS_InitScreen0(); }

// Switches VDP in SCREEN 0 mode. Wrapper for SETTXT.
inline void BIOS_SetScreen0() { Call(R_SETTXT); }

#define BIOS_InitTextMode			BIOS_InitScreen0
#define BIOS_InitTextModeColor		BIOS_InitScreen0Color
#define BIOS_InitTextModeEx			BIOS_InitScreen0Ex
#define BIOS_SetTextMode			BIOS_SetScreen0

//.............................................................................
// Screen 1

inline void BIOS_SetWidth32(u8 width) { g_LINL32 = width; }

// Switches to SCREEN 1. Wrapper for INIT32.
inline void BIOS_InitScreen1() { Call(R_INIT32); }
inline void BIOS_InitScreen1Color(u8 width, u8 text, u8 back, u8 border) { g_LINL32 = width; BIOS_SetColor(text, back, border); BIOS_InitScreen1(); }
inline void BIOS_InitScreen1Ex(u16 pnt, u16 ct, u16 pgt, u16 sat, u16 sgt, u8 width, u8 text, u8 back, u8 border) { g_T32NAM = pnt; g_T32COL = ct; g_T32CGP = pgt; g_T32ATR = sat; g_T32PAT = sgt; g_LINL32 = width; BIOS_SetColor(text, back, border); BIOS_InitScreen1(); }

// Switches VDP in SCREEN 1 mode. Wrapper for SETT32.
inline void BIOS_SetScreen1() { Call(R_SETT32); }

#define BIOS_InitText32Mode			BIOS_InitScreen1
#define BIOS_InitText32ModeColor	BIOS_InitScreen1Color
#define BIOS_InitText32ModeEx		BIOS_InitScreen1Ex
#define BIOS_SetText32Mode			BIOS_SetScreen1

//.............................................................................
// Screen 2

// Switches to SCREEN 2. Wrapper for INIGRP.
inline void BIOS_InitScreen2() { Call(R_INIGRP); }
inline void BIOS_InitScreen2Color(u8 width, u8 text, u8 back, u8 border) { g_LINL32 = width; BIOS_SetColor(text, back, border); BIOS_InitScreen2(); }
inline void BIOS_InitScreen2Ex(u16 pnt, u16 ct, u16 pgt, u16 sat, u16 sgt, u8 width, u8 text, u8 back, u8 border) { g_GRPNAM = pnt; g_GRPCOL = ct; g_GRPCGP = pgt; g_GRPATR = sat; g_GRPPAT = sgt; g_LINL32 = width; BIOS_SetColor(text, back, border); BIOS_InitScreen2(); }

// Switches VDP to SCREEN 2 mode. Wrapper for SETGRP.
inline void BIOS_SetScreen2() { Call(R_SETGRP); }

#define BIOS_InitGraphicMode		BIOS_InitScreen2
#define BIOS_InitGraphicModeColor	BIOS_InitScreen2Color
#define BIOS_InitGraphicModeEx		BIOS_InitScreen2Ex
#define BIOS_SetGraphicMode			BIOS_SetScreen2

//.............................................................................
// Screen 3

// Switches to SCREEN 3. Wrapper for INIMLT.
inline void BIOS_InitScreen3() { Call(R_INIMLT); }

// Switches to SCREEN 3 with explicit table setup. Wrapper for INIMLT.
void BIOS_InitScreen3Ex(u16 pnt, u16 ct, u16 pgt, u16 sat, u16 sgt, u8 text, u8 bg, u8 border);

// Switches VDP to SCREEN 3 mode. Wrapper for SETMLT.
inline void BIOS_SetScreen3() { Call(R_SETMLT); }

#define BIOS_InitMulticolorMode  	BIOS_InitScreen3
#define BIOS_InitMulticolorModeEx	BIOS_InitScreen3Ex
#define BIOS_SetMulticolorMode		BIOS_SetScreen3

//.............................................................................
// Sprites routines

// Initialises all sprites. Wrapper for CLRSPR.
inline void BIOS_ClearSprites() { Call(R_CLRSPR); }

#define BIOS_SPRITE_MAG_1			(0)
#define BIOS_SPRITE_MAG_2			(0b00000001)
#define BIOS_SPRITE_SIZE_8			(0)
#define BIOS_SPRITE_SIZE_16			(0b00000010)

// Set sprite size and magnification mode.
inline void BIOS_SetSpriteMode(u8 size, u8 mag) { BIOS_WriteVDP(1, (BIOS_ReadVDP(1) & 0xFC) | size | mag); }

// Returns the address of the sprite pattern table. Wrapper for CALPAT.
u16 BIOS_GetSpritePatternAddress(u8 id);

// Returns the address of the sprite attribute table. Wrapper for CALATR.
u16 BIOS_GetSpriteAttributeAddress(u8 id);

// Set the position of a sprite on the screen.
inline void BIOS_SetSpritePosition(u8 id, u8 x, u8 y) { u16 vram = BIOS_GetSpriteAttributeAddress(id); BIOS_WriteVRAM(x, vram + 1); BIOS_WriteVRAM(y, vram); }

// Hides a sprite from the screen (set vertical position to 212).
inline void BIOS_HideSprite(u8 id) { u16 vram = BIOS_GetSpriteAttributeAddress(id); BIOS_WriteVRAM(212, vram); }

// Set the pattern of a sprite.
inline void BIOS_SetSpritePattern(u8 id, u8 pat) { BIOS_WriteVRAM(pat, BIOS_GetSpriteAttributeAddress(id) + 2); }

// Set the color of a sprite.
inline void BIOS_SetSpriteColor(u8 id, u8 color) { BIOS_WriteVRAM(color, BIOS_GetSpriteAttributeAddress(id) + 3); }

// Set sprite attributes.
inline void BIOS_SetSprite(u8 id, u8 x, u8 y, u8 pat, u8 color) { u16 vram = BIOS_GetSpriteAttributeAddress(id); BIOS_WriteVRAM(y, vram); BIOS_WriteVRAM(x, vram + 1); BIOS_WriteVRAM(pat, vram + 2); BIOS_WriteVRAM(color, vram + 3); }

typedef struct BIOS_SpriteAttributes
{
	u8 y;
	u8 x;
	u8 pattern;
	u8 color;
} BIOS_SpriteAttributes;

// Set a sprite attributes from a 4-byte structure.
inline void BIOS_SetSpriteData(u8 id, const BIOS_SpriteAttributes* attrs) { BIOS_CopyToVRAM(attrs, BIOS_GetSpriteAttributeAddress(id), 4); }

// Returns current sprite size in bytes (8 or 32). Wrapper for GSPSIZ.
inline u8 BIOS_GetSpriteSize() { return ((u8(*)(void))R_GSPSIZ)(); }

// Returns FALSE if no collision occured during the previous frame.
inline bool BIOS_IsSpriteCollision() { return g_STATFL & S00_C; }

// Returns FALSE if no over-scan occured during the previous frame.
inline bool BIOS_IsSpriteOverScan() { return g_STATFL & S00_5S; }

// Returns index of the over-scanned sprite (0-31).
inline u8 BIOS_GetSpriteOverScanId() { return S00_GET_SN(g_STATFL); }

//.............................................................................
// Graph print routines

// Moves graphical cursor to the specified position.
inline void BIOS_GraphSetCursor(u16 x, u16 y) { g_GRPACX = x; g_GRPACY = y; }

// Displays a character on the graphic screen. Wrapper for GRPPRT.
inline void BIOS_GraphPrintChar(u8 chr) { CallA(R_GRPPRT, chr); }

// Displays a string on the graphic screen. Wrapper for GRPPRT.
inline void BIOS_GraphPrint(const c8* str) { while (*str) BIOS_GraphPrintChar(*str++); }

// Displays a string on the graphic screen at a position.
inline void BIOS_GraphPrintAt(u16 x, u16 y, const c8* str) { BIOS_GraphSetCursor(x, y); BIOS_GraphPrint(str); }

#if (MSX_VERSION >= MSX_2)
// Changes the operator used for graphical print.
inline void BIOS_GraphSetOperator(u8 op) { g_LOGOPR = op; }
#endif // (MSX_VERSION >= MSX_2)

#endif // BIOS_USE_VDP

//#############################################################################
// Group: Main-ROM - PSG
//#############################################################################
#if (BIOS_USE_PSG)

// Initialises PSG and sets initial value for the PLAY statement. Wrapper for GICINI.
inline void BIOS_InitPSG() { DisableInterrupt(); Call(R_GICINI); EnableInterrupt(); }

// Writes data to PSG-register. Wrapper for WRTPSG.
void BIOS_WritePSG(u8 reg, u8 value);

// Reads value from PSG-register. Wrapper for RDPSG.
inline u8 BIOS_ReadPSG(u8 reg) { return ((u8(*)(u8))R_RDPSG)(reg); }

// Begins to execute the PLAY statement as a background task. Wrapper for STRTMS.
inline void BIOS_PlayPSG() { Call(R_STRTMS); }

#endif // BIOS_USE_PSG

//#############################################################################
// Group: Main-ROM - Console
//#############################################################################

// One character input (waiting). Wrapper for CHGET.
inline c8 BIOS_GetCharacter() { return CallToA(R_CHGET); }

// Get a character input (if any) or return 0.
u8 BIOS_HasCharacter();

// Moves cursor to the specified position. Wrapper for POSIT.
void BIOS_TextSetCursor(u8 x, u8 y);

// Displays one character. Wrapper for CHPUT.
inline void BIOS_TextPrintChar(c8 chr) { CallA(R_CHPUT, chr); }

// Displays a null-terminated string.
inline void BIOS_TextPrint(const c8* str) { while (*str) BIOS_TextPrintChar(*str++); }

// Print a text at the specified position.
inline void BIOS_TextPrintAt(u8 x, u8 y, const c8* str) { BIOS_TextSetCursor(x, y); BIOS_TextPrint(str); }

// Generates beep. Wrapper for BEEP.
inline void BIOS_Beep() { Call(R_BEEP); }

// Clears the screen. Wrapper for CLS.
void BIOS_ClearScreen();

//#############################################################################
// Group: Main-ROM - Printer
//#############################################################################

// Sends one character to printer. Wrapper for LPTOUT.
inline void BIOS_PrinterSendChar(u8 chr) { CallA(R_LPTOUT, chr); }

// Sends a null-terminated string to the printer.
inline void BIOS_PrinterSendString(const u8* str) { while (*str) BIOS_PrinterSendChar(*str++); }

// Tests printer status. Wrapper for LPTSTT.
inline bool BIOS_IsPrinterReady() { return CallToA(R_LPTSTT); }

// Sends one character to printer if printer is ready. Wrapper for OUTDLP.
inline void BIOS_PrinterOutput(u8 chr) { CallA(R_OUTDLP, chr); }

// Send a change page code to the printer.
inline void BIOS_PrinterChangePage() { BIOS_PrinterSendChar(ASCII_FORM_FEED); }

//#############################################################################
// Group: Main-ROM - Controller
//#############################################################################

#define BIOS_JOYSTICK_KEYBOARD		(0)
#define BIOS_JOYSTICK_PORT1			(1)
#define BIOS_JOYSTICK_PORT2			(2)

#define BIOS_DIRECTION_NONE			(0)
#define BIOS_DIRECTION_UP			(1)
#define BIOS_DIRECTION_UP_RIGHT		(2)
#define BIOS_DIRECTION_RIGHT		(3)
#define BIOS_DIRECTION_DOWN_RIGHT	(4)
#define BIOS_DIRECTION_DOWN			(5)
#define BIOS_DIRECTION_DOWN_LEFT	(6)
#define BIOS_DIRECTION_LEFT			(7)
#define BIOS_DIRECTION_UP_LEFT		(8)

// Returns the state of the joystick or the cursor keys. Wrapper for GTSTCK.
inline u8 BIOS_GetJoystickDirection(u8 port) { return CallAToA(R_GTSTCK, port); }

#define BIOS_TRIGGER_SPACEBAR		(0)
#define BIOS_TRIGGER_PORT1_TRIG1	(1)
#define BIOS_TRIGGER_PORT2_TRIG1	(2)
#define BIOS_TRIGGER_PORT1_TRIG2	(3)
#define BIOS_TRIGGER_PORT2_TRIG2	(4)

// Returns the state of the mouse/joystick/keyboard space bar buttons. Wrapper for GTTRIG.
inline bool BIOS_GetJoystickTrigger(u8 trigger) { return CallAToA(R_GTTRIG, trigger); }

// Returns the touch pad status. Wrapper for GTPAD.
inline u8 BIOS_GetTouchPad(u8 entry) { return CallAToA(R_GTPAD, entry); }

// Returns the paddle position. Wrapper for GTPDL.
inline u8 BIOS_GetPaddle(u8 num) { return CallAToA(R_GTPDL, num); }

//#############################################################################
// Group: Main-ROM - Misc
//#############################################################################

// Returns the value of the specified line from the keyboard matrix. Wrapper for SNSMAT.
inline u8 BIOS_GetKeyboardMatrix(u8 line) { return CallAToA(R_SNSMAT, line); }

// Check if the given key is pressed.
inline bool BIOS_IsKeyPressed(u8 key) { return (g_NEWKEY[KEY_ROW(key)] & (1 << KEY_IDX(key))) == 0; }

//#############################################################################
// Group: Main-ROM - MSX turbo R
//#############################################################################
#if (MSX_VERSION == MSX_TR)

#define CPU_MODE_Z80		0x00
#define CPU_MODE_R800_ROM	0x01
#define CPU_MODE_R800_DRAM	0x02
#define CPU_TURBO_LED		0x80

// Changes CPU mode. Wrapper for CHGCPU.
inline void BIOS_SetCPUMode(u8 mode) { ((void(*)(u8))R_CHGCPU)(mode); }

// Returns current CPU mode. Wrapper for GETCPU.
inline u8 BIOS_GetCPUMode() { return ((u8(*)(void))R_GETCPU)(); }

#endif // (MSX_VERSION == MSX_TR)

#endif // MSXMVL_BIOS_H
