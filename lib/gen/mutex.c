// msxmvl — clean-room reimplementation of MSXgl "mutex" module.
//
// The entire mutex API is defined as header-only inline functions in mutex.h
// (matching MSXgl, whose mutex module is likewise a set of trivial inline
// functions). This translation unit exists so the module has a compilable .c
// alongside the rest of lib/gen; it simply pulls in the inline definitions.
//
// The mutex storage byte g_Mutex is intentionally NOT defined here: per the
// MSXgl contract it is supplied by the application via the MUTEX_DATA() macro.
//
// Build: sdcc -c -mz80 --sdcccall 1 mutex.c

#include "mutex.h"
