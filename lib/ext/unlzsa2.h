// msxmvl EXTENSION module: unlzsa2 -- runtime LZSA2 decompressor.
//
// NOT clean-room. The decoder is the size-optimized LZSA2 Z80 routine by spke &
// uniabis (from Emmanuel Marty's LZSA), transliterated to sdasz80 and wrapped for
// SDCC -- an altered version used under the zlib licence (full notice + attribution
// in unlzsa2.c). See docs/examples/unlzsa2_DOC.md for the measured size/speed.
//
// Pack your data on the host with the `lzsa` encoder (github.com/emmanuel-marty/
// lzsa) in raw LZSA2 mode -- `lzsa -f2 -r <in> <out>` -- and decompress on-target
// with UnLZSA2. "Raw" (frame-less) blocks carry their own end-of-data marker, so
// the decoder needs no length argument.
#ifndef MSXMVL_UNLZSA2_H
#define MSXMVL_UNLZSA2_H

#include "types.h"

// Decompress a raw LZSA2 stream from `src` into RAM at `dst`. The output size is
// fixed at pack time -- size `dst` yourself; there is no bounds check (matches the
// reference decoder). The stream's own EOD marker ends decompression. Overlapping
// back-references (RLE-style runs, e.g. offset 1) are handled. Runs from ROM
// (offset held in IX, no self-modifying code). NOTE: it uses the shadow
// accumulator AF' as the nibble reservoir, so it is NOT AF'-safe -- do not call it
// from a context that must preserve AF' (mask/save it around the call if needed).
void UnLZSA2(const u8* src, u8* dst);

#endif // MSXMVL_UNLZSA2_H
