// msxmvl EXTENSION module: unzx7 -- runtime ZX7 decompressor (RAM).
//
// Decompressor for the ZX7 format by Einar Saukas (format only; independent
// implementation). Clean-room: written from the ZX7 format description (LZ77/LZSS
// literals + copies, non-interlaced Elias-gamma lengths, 7-or-11-bit offsets).
// Einar Saukas's reference decoders were consulted only as a behavioural oracle;
// the code here is our own. See unzx7_DOC.md for the measured size/speed
// comparison against the reference Z80 decoder and the packing workflow.
//
// ZX7 is the predecessor of ZX0 (see unzx0.h). It is slightly weaker but its
// decoder is a touch simpler and it remains widely used. Pack your data on the
// host with the `zx7` encoder (github.com/einar-saukas/ZX7) in its default
// (forward) mode; decompress on-target with UnZX7.
#ifndef MSXMVL_UNZX7_H
#define MSXMVL_UNZX7_H

#include "types.h"

// Decompress a ZX7 stream from `src` into RAM at `dst`. The output size is fixed
// at pack time -- size `dst` yourself; there is no bounds check (matches the
// reference decoder). Unlike the reference Z80 "standard" routine (which detects
// the end marker by 16-bit length overflow and so reads a couple of bytes past
// the stream), UnZX7 detects end-of-stream by counting the 16 leading zeros, so
// it never reads beyond the last byte of the compressed block.
void UnZX7(const u8* src, u8* dst);

#endif // MSXMVL_UNZX7_H
