// msxmvl — clean-room reimplementation of MSXgl "mutex" module public API.
// Interface (names, signatures, documented behaviour) taken from MSXgl's
// engine/src/mutex.h header ONLY. No MSXgl implementation source was read.
//
// Simple 8-bit software mutex: a single byte (g_Mutex) whose 8 bits act as
// 8 independent binary locks (mutex indices 0..7). The whole module is a set
// of trivial header-only inline functions in MSXgl, so the drop-in clean-room
// equivalent is likewise header-only inline — identical semantics, no separate
// link object required. The MSXgl header declares no __PRESERVES() contract;
// these are plain C inline functions following the standard SDCC convention.
//
// g_Mutex is NOT defined by this module: the application must declare it once,
// via the MUTEX_DATA() macro (exactly as in MSXgl).
#ifndef MSXMVL_MUTEX_H
#define MSXMVL_MUTEX_H

#include "types.h"

//=============================================================================
// DEFINES
//=============================================================================

// The mutex storage. Must be declared somewhere in the application code,
// typically by expanding MUTEX_DATA() at file scope.
extern u8 g_Mutex;

// Macro: MUTEX_DATA
// Expands to the definition of the mutex storage byte. Place once in the app.
#define MUTEX_DATA() u8 g_Mutex

//=============================================================================
// FUNCTIONS
//=============================================================================

// Function: Mutex_Init
// Initialize (clear) all mutexes.
inline void Mutex_Init(void) { g_Mutex = 0; }

// Function: Mutex_Lock
// Lock the given mutex.
//
// Parameters:
//   mutex - Mutex index (0-7)
inline void Mutex_Lock(u8 mutex) { g_Mutex |= (1 << mutex); }

// Function: Mutex_Release
// Release the given mutex.
//
// Parameters:
//   mutex - Mutex index (0-7)
inline void Mutex_Release(u8 mutex) { g_Mutex &= ~(1 << mutex); }

// Function: Mutex_Wait
// Busy-wait until the given mutex is released.
//
// Parameters:
//   mutex - Mutex index (0-7)
inline void Mutex_Wait(u8 mutex) { while ((g_Mutex & (1 << mutex)) != 0); }

// Function: Mutex_Gate
// Gate test: returns TRUE if the given mutex is currently free (unlocked).
//
// Parameters:
//   mutex - Mutex index (0-7)
//
// Returns:
//   TRUE if the mutex is free, FALSE if it is locked.
inline bool Mutex_Gate(u8 mutex) { return ((g_Mutex & (1 << mutex)) == 0); }

#endif // MSXMVL_MUTEX_H
