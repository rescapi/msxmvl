// level_load.c — unpack a compressed tilemap into RAM when a level loads.
//
// Cart ROM is the scarcest thing on an MSX, so a level's tilemap ships RLEp-packed
// (run-length, packed) and is expanded on demand. RLEp_UnpackToRAM walks the packed
// stream and writes the tiles into a RAM buffer, returning how many it produced.
#include "compress.h"

// A tiny packed tilemap: [default tile] then chunks, terminated by 0x00.
static const u8 g_PackedMap[] = {
	0xAA,                    // default tile for Type-0 runs
	0x02,                    // T0 x3  -> AA AA AA
	0x43, 0xBB,              // T1 x4  -> BB BB BB BB
	0x81, 0xCC, 0xDD,        // T2 x2  -> CC DD CC DD
	0xC2, 0x11, 0x22, 0x33,  // T3 x3  -> 11 22 33 (raw)
	0x00,                    // terminator
};
static u8 g_TileMap[16];     // where the level's tiles land in RAM

// Unpack the level's tilemap into RAM; returns the tile count.
u16 load_tilemap(void)
{
	return RLEp_UnpackToRAM(g_PackedMap, g_TileMap);
}

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xE000) R[8];
void main(void)
{
	u16 len = load_tilemap();

	R[1] = (u8)len;          // 14 unpacked bytes
	R[2] = g_TileMap[0];     // 0xAA  (default run)
	R[3] = g_TileMap[3];     // 0xBB  (1-byte run)
	R[4] = g_TileMap[7];     // 0xCC  (2-byte run)
	R[5] = g_TileMap[11];    // 0x11  (raw copy)

	R[0] = (len == 14 && g_TileMap[0] == 0xAA && g_TileMap[3] == 0xBB
	     && g_TileMap[7] == 0xCC && g_TileMap[11] == 0x11) ? 0xA5 : 0x00;
	for (;;) {}
}
