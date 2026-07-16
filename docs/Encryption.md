# Encryption (`crypt`)

Turn a binary blob into a short string of printable characters, and back. This is what you use for
**password-based save states** — the classic MSX "write down this code to continue" screen — and for
lightly obfuscating high-score tables so they can't be edited with a hex editor. Link `crypt.c`,
include `crypt.h`.

> This is obfuscation, not security. The key ships inside your ROM. It stops a curious player, not
> an attacker with a disassembler.

## What it does

Each input byte becomes **exactly two characters** drawn from a 32-character map, so the encoded
string is `size * 2` characters plus a zero terminator. Three things shape the output:

| Global | Role |
|--------|------|
| `g_CryptKey` | zero-terminated key, XOR-mixed cyclically into the data |
| `g_CryptMap` | the 32 characters the output is allowed to use |
| `g_CryptCode` | 8×`u16` bit-field table spreading each input bit across the two output chars |

Encoding also **cascades**: each byte's two character indices are added onto the previous byte's, so
a repeated input byte does *not* produce a repeated output pair. That's what stops `AAAA` from
looking like `AAAA` in the code the player writes down.

## API

```c
void Crypt_SetKey(const c8* key);                     // mandatory
void Crypt_SetMap(const c8* map);                     // optional: your own 32-char alphabet
void Crypt_SetCode(const u16* code);                  // optional: your own bit-field table

bool Crypt_Encode(const void* data, u8 size, c8* str);  // needs size*2 + 1 bytes in str
bool Crypt_Decode(const c8* str, void* data);           // needs strlen(str)/2 bytes in data
```

## Making a continue code

The save screen packs the player's progress into a code they write down; the continue screen reads it
back. `make_continue_code` sets the key once and encodes the blob; `load_continue_code` decodes it
again. Four bytes become eight characters plus a terminator.

```c
// savecode.c — turn the player's progress into a short "continue" code, and back.
#include "crypt.h"

#define SAVE_BYTES  4                  // bytes of progress packed into a code

static u8 g_Progress[SAVE_BYTES] = { 0x12, 0x34, 0x56, 0x78 };
static c8 g_Code[SAVE_BYTES * 2 + 1];  // two chars per byte, plus terminator
static u8 g_Restored[SAVE_BYTES];

// Encode the current progress into a continue code the player writes down.
void make_continue_code(void)
{
	Crypt_SetKey("MSX");                    // mandatory before encode/decode
	Crypt_Encode(g_Progress, SAVE_BYTES, g_Code);
}

// Read a continue code back into the saved progress.
void load_continue_code(void)
{
	Crypt_Decode(g_Code, g_Restored);
}
```

The blob survives the round trip — every byte of `g_Restored` matches `g_Progress` — and `g_Code` is
zero-terminated at index 8. The first character of the code lands at `0x39`, a printable character
from the map. *(tested: `crypt_01_roundtrip.c`)*

## Verifying a typed-in code

A code the player types in — or edits — must be checked before you trust it. `Crypt_Decode` returns
`FALSE` the moment it meets a character that isn't in the alphabet, so a single verb decides whether
to load the save or show "WRONG CODE".

```c
// verify.c — accept a typed-in continue code only if it decodes cleanly.
#include "crypt.h"

// Verify + decode a continue code. FALSE means "reject it, show WRONG CODE".
bool verify_code(const c8* code, u8* progress)
{
	return Crypt_Decode(code, progress);
}
```

A clean code decodes and returns `TRUE`; flip one character to something outside the map and the
same call returns `FALSE` instead of handing back garbage — which is how you catch a typo or a
tampered code. The test harness drives this through the awkward edges (no key set, a one-character
key that wraps on every byte, zeros in the data, and a caller-supplied map and code table).
*(tested: `crypt_02_verify.c`)*

## Notes

- **Set the key first.** `Crypt_Encode`/`Crypt_Decode` read `g_CryptKey` on every call; with no key
  set you are XOR-ing against a dangling pointer.
- **Size the output buffer.** Encoding needs `size * 2 + 1` bytes — the terminator is the `+ 1`, and
  forgetting it is the easy way to corrupt the variable that follows.
- **Encode and decode with the same key, map and code table**, or the round trip silently returns
  different bytes rather than failing.
- Both functions return `FALSE` on failure — for example, decoding a string containing a character
  that isn't in the map (a typo'd password). Check it, and show the player "wrong code" rather than
  loading garbage.

## See also

- [Memory Operations](Memory-Operations.md) — assembling the blob you encode.
- [Text Output](Text-Output.md) — showing the code to the player.
