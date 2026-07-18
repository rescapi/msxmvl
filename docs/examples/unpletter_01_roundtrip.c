// tile_unpack.c — expand a Pletter-compressed tileset into RAM when a level loads.
//
// Cart ROM is the scarcest thing on an MSX, so graphics ship Pletter-packed and
// are expanded on demand. UnPletter walks the packed stream (literal bytes + LZ
// back-references) into a RAM buffer. Here four identical 8-pixel tile rows follow
// each other, so Pletter stores rows 2-4 as back-references instead of 24 more
// bytes — that is the whole point of a real LZ packer over plain run-length.
#include "unpletter.h"

// Four identical 8×8 tile rows, Pletter-packed by the reference encoder: 32 → 16 bytes.
static const u8 g_PackedTiles[] = {
	0x00, 0x00, 0x18, 0x3C, 0x7E, 0x7E, 0x3C, 0x37, 0x18, 0x00, 0xEF, 0x07, 0xFF, 0xFF, 0xFF, 0xFC,
};
static u8 g_Tiles[32];   // where the unpacked tile lands in RAM

// Unpack the tileset into RAM. UnPletter masks interrupts itself (it uses IX, IY
// and the alternate register set) and leaves them disabled on return; a real
// loader re-enables them with ei once the whole asset-loading phase is done.
void unpack_tiles(void)
{
	UnPletter(g_PackedTiles, g_Tiles);
}

// ---- test harness (not shown in the docs) --------------------------------
// The Pletter oracle: a corpus packed by the REFERENCE pletter encoder — corner
// cases (1 byte, all-same, incompressible, tiles) plus 14 fuzz vectors
// (deterministic random seeds, a spread of sizes that walks the encoder through
// several offset modes, literal-heavy and match-heavy) — must round-trip
// byte-exact through UnPletter. See unpletter_corpus_gen.py.
#include "unpletter_corpus.h"
static u8 g_Out[PLT_CORPUS_MAXRAW];
volatile u8 __at(0xE000) R[8];
void main(void)
{
	u8 i, ok = 1;
	u16 j;

	unpack_tiles();

	for (i = 0; i < PLT_CORPUS_COUNT; i++) {
		const PltVec* v = &g_plt_corpus[i];
		UnPletter(v->packed, g_Out);   // UnPletter di's internally; interrupts stay off
		for (j = 0; j < v->len; j++) if (g_Out[j] != v->raw[j]) { ok = 0; break; }
	}

	R[1] = PLT_CORPUS_COUNT;   // vectors tested (18)
	R[2] = ok;                 // UnPletter round-tripped all of them
	R[3] = g_Tiles[8];         // 0x00 — second tile row equals the first
	R[0] = (ok && g_Tiles[8] == g_Tiles[0] && g_Tiles[2] == 0x3C) ? 0xA5 : 0x00;
	for (;;) {}
}
