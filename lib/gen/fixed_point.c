// msxmvl clean-room reimplementation of MSXgl "fixed_point" module.
// Implemented from the module header (interface/contract) + standard C/Z80 knowledge.
// SDCC --sdcccall 1.
//
// The fixed_point module is almost entirely compile-time macros. The only
// runtime functions are the four generic Qm.n scale converters below. They take
// a fixed-point ID whose low byte is the fractional-bit count n, and scale the
// value by 2^n (SET = value -> fixed via multiply, GET = fixed -> value via
// divide). Behaviour mirrors the documented inline definitions exactly.
//
// None of these functions declare a __PRESERVES contract, so no hand-written
// register preservation is required; plain C codegen matches the MSXgl build.

#include "fixed_point.h"

// Convert source unit 8-bit value to a Qm.n fixed point number.
i8 QMN_Set8(u16 q, i8 a)
{
	return a * (i8)(1 << (q & 0x00FF));
}

// Convert a Qm.n fixed point number to source unit 8-bit value.
i8 QMN_Get8(u16 q, i8 a)
{
	return a / (i8)(1 << (q & 0x00FF));
}

// Convert source unit 16-bit value to a Qm.n fixed point number.
i16 QMN_Set16(u16 q, i16 a)
{
	return a * (i16)(1 << (q & 0x00FF));
}

// Convert a Qm.n fixed point number to source unit 16-bit value.
i16 QMN_Get16(u16 q, i16 a)
{
	return a / (i16)(1 << (q & 0x00FF));
}
