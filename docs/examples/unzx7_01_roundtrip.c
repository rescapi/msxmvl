// tile_unpack.c — expand a ZX7-compressed tileset into RAM when a level loads.
//
// Cart ROM is the scarcest thing on an MSX, so graphics ship ZX7-packed and are
// expanded on demand. UnZX7 walks the packed stream (literal bytes + LZ back-
// references) into a RAM buffer. Here the tile's second 8-pixel row is identical
// to the first, so ZX7 stores it as a single back-reference instead of 8 more
// bytes — that is the whole point of a real LZ packer over plain run-length.
#include "unzx7.h"

// Two identical 8×8 tile rows, ZX7-packed by the reference encoder: 16 → 13 bytes.
static const u8 g_PackedTiles[] = {
	0x00, 0x01, 0x18, 0x3C, 0x7E, 0x7E, 0x3C, 0x18, 0x00, 0x3C, 0x07, 0x00, 0x02,
};
static u8 g_Tiles[16];   // where the unpacked tile lands in RAM

// Unpack the tileset into RAM.
void unpack_tiles(void)
{
	UnZX7(g_PackedTiles, g_Tiles);
}

// ---- test harness (not shown in the docs) --------------------------------
// The ZX7 oracle: a corpus packed by the REFERENCE zx7 encoder — corner cases
// (1 byte, all-same, incompressible) plus 24 fuzz vectors (deterministic random
// seeds, a spread of sizes, literal-heavy and match-heavy) — must round-trip
// byte-exact through UnZX7. See unzx7_corpus_gen.py.
#include "unzx7_corpus.h"
static u8 g_Out[ZX7_CORPUS_MAXRAW];
volatile u8 __at(0xE000) R[8];
void main(void)
{
	u8 i, ok = 1;
	u16 j;

	unpack_tiles();           // the doc snippet must land the tile in g_Tiles

	for (i = 0; i < ZX7_CORPUS_COUNT; i++) {
		const ZX7Vec* v = &g_zx7_corpus[i];
		UnZX7(v->packed, g_Out);
		for (j = 0; j < v->len; j++) if (g_Out[j] != v->raw[j]) { ok = 0; break; }
	}

	R[1] = ZX7_CORPUS_COUNT;  // vectors tested (30)
	R[2] = ok;                // UnZX7 round-tripped all of them
	R[3] = g_Tiles[8];        // 0x00 — second tile row equals the first
	R[0] = (ok && g_Tiles[8] == g_Tiles[0] && g_Tiles[2] == 0x3C)
	       ? 0xA5 : 0x00;
	for (;;) {}
}
