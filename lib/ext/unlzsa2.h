// msxmvl EXTENSION module: unlzsa2 -- runtime LZSA2 decompressor (RAM).
//
// Decompressor for the LZSA2 format by Emmanuel Marty (format only; independent
// implementation). Clean-room: the RAM decoder was written from the LZSA2 block
// format description (token XYZ|LL|MMM, nibble-packed extra lengths, 5/9/13/16-bit
// and repeat match offsets stored as negative values). The reference `lzsa`
// compressor and the spke/uniabis Z80 decoders were consulted only for FACTS
// (the format) and as a behavioural oracle; the code here is our own. See
// docs/examples/unlzsa2_DOC.md for the measured size/speed comparison.
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
// back-references (RLE-style runs, e.g. offset 1) are handled. Runs with
// interrupts left as-is and uses only the main register set (no shadow registers),
// so it is safe to call from an interrupt-driven program without masking AF'.
void UnLZSA2(const u8* src, u8* dst);

#endif // MSXMVL_UNLZSA2_H
