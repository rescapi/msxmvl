// tile_unpack.c — expand an LZSA2-compressed tileset into RAM when a level loads.
//
// Cart ROM is the scarcest thing on an MSX, so graphics ship LZSA2-packed and are
// expanded on demand. UnLZSA2 walks the packed stream (literal bytes + LZ back-
// references) into a RAM buffer. Here the tile's second 8-pixel row is identical
// to the first, so LZSA2 stores it as a single back-reference instead of 8 more
// bytes — that is the whole point of a real LZ packer over plain run-length.
#include "unlzsa2.h"

// Two identical 8×8 tile rows, LZSA2-packed by the reference encoder: 16 → 13 bytes.
static const u8 g_PackedTiles[] = {
	0x3E, 0x5C, 0x00, 0x18, 0x3C, 0x7E, 0x7E, 0x3C, 0x18, 0x00, 0xE7, 0xF0, 0xE8,
};
static u8 g_Tiles[16];   // where the unpacked tile lands in RAM

// Unpack the tileset into RAM.
void unpack_tiles(void)
{
	UnLZSA2(g_PackedTiles, g_Tiles);
}

// ---- test harness (not shown in the docs) --------------------------------
// The LZSA2 oracle: a corpus packed by the REFERENCE `lzsa -f2 -r` encoder —
// corner cases (1 byte, all-same, 4 KB zero, incompressible, tile data) plus fuzz
// vectors (deterministic seeds, a spread of sizes, literal-heavy and match-heavy) —
// must round-trip byte-exact. See unlzsa2_corpus_gen.py.
#include "unlzsa2_corpus.h"
static u8 g_Out[LZSA2_CORPUS_MAXRAW];
volatile u8 __at(0xE000) R[8];
void main(void)
{
	u8 i, ok = 1;
	u16 j;

	unpack_tiles();

	for (i = 0; i < LZSA2_CORPUS_COUNT; i++) {
		const LZSA2Vec* v = &g_lz2_corpus[i];
		UnLZSA2(v->packed, g_Out);
		for (j = 0; j < v->len; j++)
			if (g_Out[j] != v->raw[j]) { ok = 0; break; }
		if (!ok) break;
	}

	R[1] = LZSA2_CORPUS_COUNT;  // vectors tested
	R[2] = ok;                  // UnLZSA2 round-tripped all of them
	R[3] = i;                   // index of the first failure (== COUNT if none)
	R[4] = g_Tiles[8];          // 0x00 — second tile row equals the first
	R[0] = (ok && g_Tiles[8] == g_Tiles[0] && g_Tiles[2] == 0x3C)
	       ? 0xA5 : 0x00;
	for (;;) {}
}
