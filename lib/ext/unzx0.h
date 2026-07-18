// msxmvl EXTENSION module: unzx0 -- runtime ZX0 decompressor (RAM + VRAM).
//
// Decompressor for the ZX0 format by Einar Saukas (format only; independent
// implementation). Clean-room: written from the ZX0 v2 format description
// (interlaced Elias-gamma lengths + LZ literals/copies with last-offset reuse).
// Einar Saukas's reference decoder was consulted only as a behavioural oracle;
// the code here is our own. See docs/Compression.md for the measured size/speed
// comparison against the reference and the packing workflow.
//
// Pack your data on the host with the `zx0` encoder (github.com/einar-saukas/ZX0)
// in its default (forward, v2) mode; decompress on-target with these functions.
#ifndef MSXMVL_UNZX0_H
#define MSXMVL_UNZX0_H

#include "types.h"

// Decompress a ZX0 stream from `src` into RAM at `dst`. The output size is fixed
// at pack time -- size `dst` yourself; there is no bounds check (matches the
// reference decoder). Runs with interrupts left as-is; it uses the alternate AF
// register, so an interrupt handler that clobbers AF' must be masked around the
// call (again, the reference decoder's contract).
void UnZX0(const u8* src, u8* dst);

// Faster RAM decoder: byte-identical output to UnZX0, ~19% fewer T-states (128 B
// vs 69 B). It inlines the Elias reads and keeps the last match offset in a RAM
// word instead of on the stack. This is the "turbo" decode shape, but ROM-SAFE:
// unlike Einar Saukas's turbo (which self-modifies its own code and must run from
// RAM), UnZX0_fast keeps that slot in RAM DATA, so the decoder itself runs fine
// from ROM. Costs one 2-byte RAM global (g_ZX0_FastOff) and, like UnZX0, uses the
// alternate AF register. Use it when the extra ~59 bytes buy worthwhile speed.
void UnZX0_fast(const u8* src, u8* dst);

// Decompress a ZX0 stream from `src` straight into VRAM at 14-bit address
// `vramDst`, producing `len` bytes (the original uncompressed size). Back-
// references are resolved by reading VRAM back, so the whole output must lie
// within one 16 KB VRAM page (auto-increment does not cross a page). Masks
// interrupts for the duration to keep the VDP address latch coherent. Works on
// MSX1 (TMS9918) and MSX2 (V99x8).
void UnZX0_VRAM(const u8* src, u16 vramDst, u16 len);

#endif // MSXMVL_UNZX0_H
