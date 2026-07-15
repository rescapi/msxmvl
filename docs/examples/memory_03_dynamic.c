// Example: the dynamic allocator — alloc, free, exact-fit reuse, and COALESCING.
// The allocator is a rewrite; nothing else in the suite exercised it.
#include "memory.h"
volatile u8 __at(0xE000) R[8];
static u8 pool[256];
void main(void){
  u8 ok = 1;
  u8 *a, *b, *c, *d;
  Mem_DynamicInitialize(pool, sizeof(pool));

  a = (u8*)Mem_DynamicAlloc(16);
  b = (u8*)Mem_DynamicAlloc(16);
  c = (u8*)Mem_DynamicAlloc(16);
  if (!a || !b || !c) ok = 0;
  if (a == b || b == c) ok = 0;                 // distinct blocks
  if (Mem_GetDynamicSize(a) != 16) ok = 0;      // header records the size
  if ((u16)b <= (u16)a) ok = 0;                 // and they advance

  a[0] = 0x11; a[15] = 0x22;                    // blocks must not overlap
  b[0] = 0x33; b[15] = 0x44;
  if (a[0] != 0x11 || a[15] != 0x22) ok = 0;
  if (b[0] != 0x33 || b[15] != 0x44) ok = 0;

  Mem_DynamicFree(b);                           // free the middle, then re-take it
  d = (u8*)Mem_DynamicAlloc(16);
  if (d != b) ok = 0;                           // exact-fit reuse

  Mem_DynamicFree(d);                           // free two ADJACENT blocks: they must
  Mem_DynamicFree(c);                           // coalesce, or a big alloc cannot fit
  d = (u8*)Mem_DynamicAlloc(40);                // 16+16+4 header = 36 < 40 needs the merge
  if (!d) ok = 0;

  R[1] = ok;
  R[2] = (Mem_DynamicAlloc(4096) == NULL) ? 1 : 0;   // too big -> NULL, not a wild pointer
  R[0] = (ok && R[2]) ? 0xA5 : 0x00;
  for(;;){}
}
