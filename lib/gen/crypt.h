// msxmvl — clean-room reimplementation of MSXgl "crypt" module public API.
// Interface (names, signatures, documented behaviour) taken from MSXgl's
// engine/src/crypt.h header ONLY. No MSXgl implementation source was read.
//
// Data encryption and decryption module.
//   - Each source byte is emitted as exactly 2 characters (output size = size*2+1).
//   - g_CryptKey : zero-terminated key, XOR-mixed (cyclically) into the data.
//   - g_CryptMap : the valid output characters (default = 32 chars).
//   - g_CryptCode: 8 entries of u16; entry [bit] is a bit-field that maps input
//                  bit 'bit' into the two output characters (low byte -> char 1,
//                  high byte -> char 2).  Each entry is a single, distinct bit
//                  mask, which makes the transform directly invertible.
//   - v1.1 "cascaded translation": each byte's two character indices are added
//                  (mod map length) onto the previous byte's indices.
#ifndef MSXMVL_CRYPT_H
#define MSXMVL_CRYPT_H

#include "types.h"

//=============================================================================
// GLOBALS
//=============================================================================

extern const c8*  g_CryptKey;  // Zero-terminated string used as key
extern const c8*  g_CryptMap;  // String containing the 32 valid characters
extern const u16* g_CryptCode; // Bit-field coding table (8 x u16)

//=============================================================================
// FUNCTIONS
//=============================================================================

// Function: Crypt_SetKey
// Set the encryption key (mandatory before Crypt_Encode / Crypt_Decode).
inline void Crypt_SetKey(const c8* key) { g_CryptKey = key; }

// Function: Crypt_SetMap
// Set the encryption character mapping table (optional).
inline void Crypt_SetMap(const c8* map) { g_CryptMap = map; }

// Function: Crypt_SetCode
// Set the encryption code bit field (optional).
inline void Crypt_SetCode(const u16* code) { g_CryptCode = code; }

// Function: Crypt_Encode
// Encrypt 'size' bytes from 'data' into 'str' (needs size*2+1 bytes).
// Returns FALSE if encoding failed.
bool Crypt_Encode(const void* data, u8 size, c8* str);

// Function: Crypt_Decode
// Decrypt zero-terminated 'str' into 'data' (needs strlen(str)/2 bytes).
// Returns FALSE if decoding failed.
bool Crypt_Decode(const c8* str, void* data);

#endif // MSXMVL_CRYPT_H
