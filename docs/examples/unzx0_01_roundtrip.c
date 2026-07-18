// tile_unpack.c — expand a ZX0-compressed tileset into RAM when a level loads.
//
// Cart ROM is the scarcest thing on an MSX, so graphics ship ZX0-packed and are
// expanded on demand. UnZX0 walks the packed stream (literal bytes + LZ back-
// references) into a RAM buffer. Here the tile's second 8-pixel row is identical
// to the first, so ZX0 stores it as a single back-reference instead of 8 more
// bytes — that is the whole point of a real LZ packer over plain run-length.
#include "unzx0.h"

// Two identical 8×8 tile rows, ZX0-packed by the reference encoder: 16 → 13 bytes.
static const u8 g_PackedTiles[] = {
	0x0A, 0x00, 0x18, 0x3C, 0x7E, 0x7D, 0x3C, 0x18, 0x00, 0xF0, 0xC0, 0x00, 0x20,
};
static u8 g_Tiles[16];   // where the unpacked tile lands in RAM

// Unpack the tileset into RAM.
void unpack_tiles(void)
{
	UnZX0(g_PackedTiles, g_Tiles);
}

// The same, on the fast path: byte-identical output, ~19% fewer cycles, still ROM-safe.
void unpack_tiles_fast(void)
{
	UnZX0_fast(g_PackedTiles, g_Tiles);
}

// ---- test harness (not shown in the docs) --------------------------------
// The ZX0 oracle: a corpus packed by the REFERENCE zx0 encoder — corner cases
// (1 byte, all-same, incompressible) plus 24 fuzz vectors (deterministic random
// seeds, a spread of sizes, literal-heavy and match-heavy) — must round-trip
// byte-exact through BOTH decoders. See unzx0_corpus_gen.py.
#include "unzx0_corpus.h"
static u8 g_Out[ZX0_CORPUS_MAXRAW];
volatile u8 __at(0xE000) R[8];
void main(void)
{
	u8 i, okStd = 1, okFast = 1;
	u16 j;

	unpack_tiles();
	unpack_tiles_fast();      // both wrappers must land the same bytes in g_Tiles

	for (i = 0; i < ZX0_CORPUS_COUNT; i++) {
		const ZX0Vec* v = &g_zx0_corpus[i];
		UnZX0(v->packed, g_Out);
		for (j = 0; j < v->len; j++) if (g_Out[j] != v->raw[j]) { okStd = 0; break; }
		UnZX0_fast(v->packed, g_Out);
		for (j = 0; j < v->len; j++) if (g_Out[j] != v->raw[j]) { okFast = 0; break; }
	}

	R[1] = ZX0_CORPUS_COUNT;  // vectors tested (30)
	R[2] = okStd;             // UnZX0      round-tripped all of them
	R[3] = okFast;            // UnZX0_fast round-tripped all of them
	R[4] = g_Tiles[8];        // 0x00 — second tile row equals the first
	R[0] = (okStd && okFast && g_Tiles[8] == g_Tiles[0] && g_Tiles[2] == 0x3C)
	       ? 0xA5 : 0x00;
	for (;;) {}
}
