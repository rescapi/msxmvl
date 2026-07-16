// levelheap.c — a tiny heap for per-level scratch buffers.
//
// When a level loads, grab the buffers it needs from a fixed pool; hand them back
// when the level ends, and the freed space is reused (and coalesced) by the next
// level. The allocator records each block's size in a small header, exact-fits a
// freed block on reuse, and merges adjacent free blocks so big requests still fit.
#include "memory.h"

// Set up the level heap over a fixed pool of RAM (once, when the game boots).
void level_heap_init(u8* pool, u16 size)
{
	Mem_DynamicInitialize(pool, size);
}

// Grab a scratch buffer of `size` bytes (NULL if the heap can't fit it).
u8* level_alloc(u16 size)
{
	return (u8*)Mem_DynamicAlloc(size);
}

// Hand a buffer back so a later level can reuse the space.
void level_free(u8* buf)
{
	Mem_DynamicFree(buf);
}

// How big is this buffer? (read straight from the block's header.)
u16 level_block_size(u8* buf)
{
	return Mem_GetDynamicSize(buf);
}

// ---- test harness (not shown in the docs) --------------------------------
volatile u8 __at(0xE000) R[8];
static u8 pool[256];
void main(void){
  u8 ok = 1;
  u8 *a, *b, *c, *d;
  level_heap_init(pool, sizeof(pool));

  a = level_alloc(16);
  b = level_alloc(16);
  c = level_alloc(16);
  if (!a || !b || !c) ok = 0;
  if (a == b || b == c) ok = 0;                 // distinct blocks
  if (level_block_size(a) != 16) ok = 0;        // header records the size
  if ((u16)b <= (u16)a) ok = 0;                 // and they advance

  a[0] = 0x11; a[15] = 0x22;                    // blocks must not overlap
  b[0] = 0x33; b[15] = 0x44;
  if (a[0] != 0x11 || a[15] != 0x22) ok = 0;
  if (b[0] != 0x33 || b[15] != 0x44) ok = 0;

  level_free(b);                                // free the middle, then re-take it
  d = level_alloc(16);
  if (d != b) ok = 0;                           // exact-fit reuse

  level_free(d);                                // free two ADJACENT blocks: they must
  level_free(c);                                // coalesce, or a big alloc cannot fit
  d = level_alloc(40);                          // 16+16+4 header = 36 < 40 needs the merge
  if (!d) ok = 0;

  R[1] = ok;
  R[2] = (level_alloc(4096) == NULL) ? 1 : 0;   // too big -> NULL, not a wild pointer
  R[0] = (ok && R[2]) ? 0xA5 : 0x00;
  for(;;){}
}
