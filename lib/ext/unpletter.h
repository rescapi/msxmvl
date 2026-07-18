// msxmvl EXTENSION module: unpletter -- runtime Pletter decompressor (RAM).
//
// Decompressor for the Pletter v0.5c format by XL2S Entertainment (format only;
// independent implementation). Clean-room: the decoder was written from the
// Pletter v0.5c stream format (a 3-bit mode header selecting the offset width,
// literal bytes, interlaced Elias-gamma match lengths, and a 34-bit end marker).
// The reference Z80 depacker was consulted only as a behavioural oracle; the
// code here is our own. See unpletter_DOC.md for the measured size/speed
// comparison against the reference depacker and the packing workflow.
//
// Pack your data on the host with the reference `pletter` encoder
// (github.com/nanochess/Pletter, or MSXgl's vendored copy) in its DEFAULT mode
// -- WITHOUT the -s "save length" flag, since this decoder stops on the stream's
// own end marker rather than a stored size. Decompress on-target with UnPletter.
#ifndef MSXMVL_UNPLETTER_H
#define MSXMVL_UNPLETTER_H

#include "types.h"

// Decompress a Pletter v0.5c stream from `src` into RAM at `dst`. The output size
// is fixed at pack time -- size `dst` yourself; there is no bounds check (matches
// the reference depacker). It uses IX, IY and the alternate register set (exx) for
// the whole decode, so it does `di` on entry and, because the decode ends in the
// alternate register bank (where re-enabling interrupts would let a bank-swapping
// ISR corrupt the caller), it LEAVES INTERRUPTS DISABLED on return. Re-enable them
// with `ei` once your asset-loading phase is finished.
void UnPletter(const u8* src, u8* dst);

#endif // MSXMVL_UNPLETTER_H
