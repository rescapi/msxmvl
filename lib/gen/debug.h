// msxmvl — clean-room reimplementation of MSXgl "debug" module public API.
// Interface (names, signatures, documented behaviour) taken from MSXgl's
// engine/src/debug.h header ONLY. No MSXgl implementation source was read.
//
// The debug module is a set of helper functions that talk to an external
// debugger/profiler tool through the MSX I/O ports. msxmvl targets the
// openMSX "PVM" debugger script (DEBUG_OPENMSX_P, tools/script/openMSX/
// debugger_pvm.tcl), whose wire protocol is fully documented by that tcl:
//
//   * Port 0x2E (control): writing 0xFF requests a break; any other value
//     selects the "debug_mode" used by the "%?" specifier.
//   * Port 0x2F (data): the printf channel. Two consecutive writes deliver
//     the 16-bit address (low byte first, then high byte) of an argument
//     block laid out in memory as:
//         [format-string pointer : 2 bytes][arg0][arg1]...
//     Each 16-bit / char / pointer argument occupies 2 bytes; each 32-bit /
//     float argument occupies 4 bytes. The tool reads that block straight
//     out of RAM at the instant of the second write.
//
// Because SDCC (--sdcccall 1) passes a variadic function's parameters
// contiguously on the stack, DEBUG_PRINT simply hands the tool a pointer to
// its own on-stack parameter list — no copy required.
#ifndef MSXMVL_DEBUG_H
#define MSXMVL_DEBUG_H

#include "types.h"

//=============================================================================
// COMPILER GLUE
//=============================================================================

#ifndef __NAKED
#define __NAKED			__naked
#endif
#ifndef __PRESERVES
#define __PRESERVES		__preserves_regs
#endif

//=============================================================================
// DEFINES
//=============================================================================

// DEBUG_TOOL selector values (mirrors MSXgl config_option.h).
#define DEBUG_NONE			0x00
#define DEBUG_DEVICE		0x10
#define DEBUG_EMULICIOUS	0x20
#define DEBUG_OPENMSX		0x30
#define DEBUG_OPENMSX_P		0x31
#define DEBUG_DISABLE		DEBUG_NONE

#ifndef DEBUG_TOOL
#define DEBUG_TOOL			DEBUG_OPENMSX_P
#endif

// PVM / debugdevice I/O ports.
#define P_DEBUG_CTRL		0x2E	// control port (break / debug_mode)
#define P_DEBUG_DATA		0x2F	// printf data channel

// Value written to the control port to request a break.
#define DEBUG_BREAK_CODE	0xFF

//=============================================================================
// GROUP: DEBUG
//=============================================================================

// Function: DEBUG_INIT
// Initialize Debug module. On openMSX this resets the tool's debug_mode to a
// known state (writes 0 to the control port).
void DEBUG_INIT();

// Function: DEBUG_BREAK
// Force a break point (writes DEBUG_BREAK_CODE to the control port).
void DEBUG_BREAK();

// Function: DEBUG_ASSERT
// Conditional break: a 'false' (zero) argument triggers a break.
//   a - value to evaluate.
void DEBUG_ASSERT(bool a);

// Function: DEBUG_LOG
// Display a nul-terminated message followed by a new line.
//   msg - message to display.
void DEBUG_LOG(const c8* msg);

// Function: DEBUG_LOGNUM
// Display a message and an 8-bit value, followed by a new line.
//   msg - message to display.
//   num - 8-bit value to display.
void DEBUG_LOGNUM(const c8* msg, u8 num);

// Function: DEBUG_PRINT
// Display a formatted message (no trailing new line is added).
//   format - nul-terminated format string, followed by its arguments.
void DEBUG_PRINT(const c8* format, ...);

//=============================================================================
// GROUP: PROFILE (header-only, no external tool selected)
//=============================================================================
// Provided so the public surface matches MSXgl when no PROFILE_TOOL is set.

// Function: PROFILE_FRAME_START
inline void PROFILE_FRAME_START() {}

// Function: PROFILE_FRAME_END
inline void PROFILE_FRAME_END() {}

// Function: PROFILE_SECTION_START
inline void PROFILE_SECTION_START(u8 level, u8 section, const c8* msg) { level; section; msg; }

// Function: PROFILE_SECTION_END
inline void PROFILE_SECTION_END(u8 level, u8 section, const c8* msg) { level; section; msg; }

#endif // MSXMVL_DEBUG_H
