// msxmvl clean-room reimplementation of MSXgl "compress" module.
// Drop-in: exposes MSXgl's exact public names + signatures.
#ifndef MSXMVL_COMPRESS_H
#define MSXMVL_COMPRESS_H

#include "types.h"

//=============================================================================
// CONFIGURATION
//=============================================================================
// These mirror the MSXgl msxgl_config.h defaults so the driver and library
// agree on the RLEp_UnpackToRAM signature. Override before including if needed.

#ifndef COMPRESS_USE_RLEP
	#define COMPRESS_USE_RLEP			1	// Use RLEp unpacker
#endif
#ifndef COMPRESS_USE_RLEP_DEFAULT
	#define COMPRESS_USE_RLEP_DEFAULT	1	// Data include the default value (0 otherwise)
#endif
#ifndef COMPRESS_USE_RLEP_FIXSIZE
	#define COMPRESS_USE_RLEP_FIXSIZE	0	// Give the data size as function input (loop up to terminator otherwise)
#endif

//=============================================================================
// RLEp
//=============================================================================
#if (COMPRESS_USE_RLEP)

#if (COMPRESS_USE_RLEP_FIXSIZE)
	#define RLEP_FIXSIZE_PARAM	, u8 size
#else
	#define RLEP_FIXSIZE_PARAM
#endif

//-----------------------------------------------------------------------------
// RLEp Chunk Header
//-----------------------------------------------------------------------------
//	7	6	5	4	3	2	1	0
//	T1	T0	C5	C4	C3	C2	C1	C0
//	│	│	└───┴───┴───┴───┴───┴── Count (0-63). Must be incremented to get the right value.
//	└───┴────────────────────────── Type (0-3).
//									0: A sequence of the default value (zeros).
//									1: A sequence of a given 1 byte (provided after the header).
//									2: A sequence of a given 2 bytes (provided after the header).
//									3: A sequence of uncompressed data (provided after the header).

// Function: RLEp_UnpackToRAM
// Unpack RLEp compressed data to memory
//
// Parameters:
//   src - Source data
//   dst - Destination data in RAM
//   size - Size of the data to unpack (only if COMPRESS_USE_RLEP_FIXSIZE is defined)
//
// Return:
//   Unpacked data size
u16 RLEp_UnpackToRAM(const u8* src, u8* dst RLEP_FIXSIZE_PARAM);

#endif // (COMPRESS_USE_RLEP)

#endif // MSXMVL_COMPRESS_H
