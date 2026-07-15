// msxmvl clean-room reimplementation of MSXgl "bios_hook" module.
// Drop-in: exposes MSXgl's exact public names + signatures.
//
// BIOS hooks routines handler.
// References:
// - https://www.msx.org/wiki/System_hooks
#ifndef MSXMVL_BIOS_HOOK_H
#define MSXMVL_BIOS_HOOK_H

//=============================================================================
// INCLUDES
//=============================================================================

#include "types.h"
#include "memory.h"

//=============================================================================
// DEFINES
//=============================================================================

// Callback default signature (matches MSXgl core.h)
#ifndef MSXMVL_CALLBACK_T
#define MSXMVL_CALLBACK_T
typedef void (*callback)(void);
#endif

//=============================================================================
// FUNCTIONS
//=============================================================================

//-----------------------------------------------------------------------------
// Hooks handler

// Function: BIOS_SetHookCallback
// Set a safe hook interslot jump to a given function.
// Installs an RST #30 (CALSLT) inter-slot jump at the hook, targeting the
// callback in whatever slot currently pages in the callback's address.
//
// Parameters:
//   hook - Hook address
//   cb - Function to be called
void BIOS_SetHookCallback(u16 hook, callback cb);

// Function: BIOS_SetHookDirectCallback
// Set a Hook to jump directly to a given function.
//
// Parameters:
//   hook - Hook address
//   cb - Function to be called
inline void BIOS_SetHookDirectCallback(u16 hook, callback cb)
{
	*((u8*)hook) = 0xC3; // JUMP
	*((callback*)++hook) = cb; // Callback address
}

// Function: BIOS_SetHookInterSlotCallback
// Set a Hook to jump to a given function in a given slot ID.
//
// Parameters:
//   hook - Hook address
//   slot - Slot ID
//   cb - Function to be called
inline void BIOS_SetHookInterSlotCallback(u16 hook, u32 slot, callback cb)
{
	*((u8*)hook) = 0xF7; // RST #30
	*((u8*)++hook) = slot; // Slot ID
	*((callback*)++hook) = cb; // Callback address
}

// Function: BIOS_ClearHook
// Clear a Hook (set RET asm code).
//
// Parameters:
//   hook - Hook address
inline void BIOS_ClearHook(u16 hook)
{
	*((u8*)hook) = 0xC9; // RET
}

// Function: BIOS_BackupHook
// Backup hook content into a buffer (must be 5 bytes length). The buffer
// content can be called at any time using Call() function.
//
// Parameters:
//   hook - Hook address
//   buffer - Destination buffer in RAM
inline void BIOS_BackupHook(u16 hook, void* buffer)
{
	Mem_Copy((void*)hook, (void*)buffer, 5);
}

//-----------------------------------------------------------------------------
// Hooks addresses

#define H_KEYI	0xFD9A
#define H_TIMI	0xFD9F
#define H_CHPU	0xFDA4
#define H_DSPC	0xFDA9
#define H_ERAC	0xFDAE
#define H_DSPF	0xFDB3
#define H_ERAF	0xFDB8
#define H_TOTE	0xFDBD
#define H_CHGE	0xFDC2
#define H_INIP	0xFDC7
#define H_KEYC	0xFDCC
#define H_KYEA	0xFDD1
#define H_NMI	0xFDD6
#define H_PINL	0xFDDB
#define H_QINL	0xFDE0
#define H_INLI	0xFDE5
#define H_ONGO	0xFDEA
#define H_DSKO	0xFDEF
#define H_SETS	0xFDF4
#define H_NAME	0xFDF9
#define H_KILL	0xFDFE
#define H_IPL	0xFE03
#define H_COPY	0xFE08
#define H_CMD	0xFE0D
#define H_DSKF	0xFE12
#define H_DSKI	0xFE17
#define H_ATTR	0xFE1C
#define H_LSET	0xFE21
#define H_RSET	0xFE26
#define H_FIEL	0xFE2B
#define H_MKI	0xFE30
#define H_MKS	0xFE35
#define H_MKD	0xFE3A
#define H_CVI	0xFE3F
#define H_CVS	0xFE44
#define H_CVD	0xFE49
#define H_GETP	0xFE4E
#define H_SETF	0xFE53
#define H_NOFO	0xFE58
#define H_NULO	0xFE5D
#define H_NTFL	0xFE62
#define H_MERG	0xFE67
#define H_SAVE	0xFE6C
#define H_BINS	0xFE71
#define H_BINL	0xFE76
#define H_FILE	0xFE7B
#define H_DGET	0xFE80
#define H_FILO	0xFE85
#define H_INDS	0xFE8A
#define H_RSLF	0xFE8F
#define H_SAVD	0xFE94
#define H_LOC	0xFE99
#define H_LOF	0xFE9E
#define H_EOF	0xFEA3
#define H_FPOS	0xFEA8
#define H_BAKU	0xFEAD
#define H_PARD	0xFEB2
#define H_NODE	0xFEB7
#define H_POSD	0xFEBC
#define H_DEVN	0xFEC1
#define H_GEND	0xFEC6
#define H_RUNC	0xFECB
#define H_CLEA	0xFED0
#define H_LOPD	0xFED5
#define H_STKE	0xFEDA
#define H_ISFL	0xFEDF
#define H_OUTD	0xFEE4
#define H_CRDO	0xFEE9
#define H_DSKC	0xFEEE
#define H_DOGR	0xFEF3
#define H_PRGE	0xFEF8
#define H_ERRP	0xFEFD
#define H_ERRF	0xFF02
#define H_READ	0xFF07
#define H_MAIN	0xFF0C
#define H_DIRD	0xFF11
#define H_FINI	0xFF16
#define H_FINE	0xFF1B
#define H_CRUN	0xFF20
#define H_CRUS	0xFF25
#define H_ISRE	0xFF2A
#define H_NTFN	0xFF2F
#define H_NOTR	0xFF34
#define H_SNGF	0xFF39
#define H_NEWS	0xFF3E
#define H_GONE	0xFF43
#define H_CNRG	0xFF48
#define H_RETU	0xFF4D
#define H_PRTF	0xFF52
#define H_COMP	0xFF57
#define H_FINP	0xFF5C
#define H_TRMN	0xFF61
#define H_FRME	0xFF66
#define H_NTPL	0xFF6B
#define H_EVAL	0xFF70
#define H_OKNO	0xFF75
#define H_MDIN	0xFF75
#define H_FING	0xFF7A
#define H_ISMI	0xFF7F
#define H_WIDT	0xFF84
#define H_LIST	0xFF89
#define H_BUFL	0xFF8E
#define H_FRQINT	0xFF93
#define H_MDTM	0xFF93
#define H_SCNE	0xFF98
#define H_FRET	0xFF9D
#define H_PTRG	0xFFA2
#define H_PHYD	0xFFA7
#define H_FORM	0xFFAC
#define H_ERRO	0xFFB1
#define H_LPTO	0xFFB6
#define H_LPTS	0xFFBB
#define H_SCRE	0xFFC0
#define H_PLAY	0xFFC5
#define E_TBIO	0xFFCA
#define H_BGFD	0xFFCF
#define H_ENFD	0xFFD4

#endif // MSXMVL_BIOS_HOOK_H
