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

## Round-tripping a save blob

```c
#include "crypt.h"
volatile u8 __at(0xE000) R[8];

static u8 g_Score[4] = { 0x12, 0x34, 0x56, 0x78 };
static c8 g_Cipher[4 * 2 + 1];   // size*2 + 1
static u8 g_Back[4];

void main(void)
{
	u8 i, ok = 1;

	Crypt_SetKey("MSX");                    // mandatory before encode/decode
	Crypt_Encode(g_Score, 4, g_Cipher);     // 4 bytes -> 8 chars + terminator
	Crypt_Decode(g_Cipher, g_Back);         // and back again

	for (i = 0; i < 4; i++)
		if (g_Back[i] != g_Score[i])
			ok = 0;

	R[1] = (u8)g_Cipher[0];    // a printable char from the map
	R[2] = g_Back[0];          // 0x12 — recovered
	R[3] = g_Back[3];          // 0x78 — recovered

	R[0] = (ok && g_Cipher[8] == 0) ? 0xA5 : 0x00;
	for (;;) {}
}
```

Runs to `R[] = a5 33 12 78` — the blob survives the round trip, and the cipher text is properly
zero-terminated at index 8. *(tested: `crypt_01_roundtrip.c`)*

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
