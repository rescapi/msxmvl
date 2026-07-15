// msxmvl — clean-room reimplementation of MSXgl "extbios" module.
//
// The MSXgl "extbios" module is header-only: its entire public API is the
// inline function ExtBIOS_Check() (plus the EXTBIO/HOKVLD absolute-address
// globals and the manufacturer-id constants). There are therefore no
// out-of-line symbols to emit here; this translation unit exists so the
// module has a compilable .c and so the header is self-consistent.

#include "extbios.h"
