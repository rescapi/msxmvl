// Far-call cost measurement. Three intervals bracketed by PC markers _m0.._m3,
// all doing the SAME trivial work (return a constant) so the leaf body cancels
// and the deltas isolate call overhead:
//   _m0.._m1  near call to a RESIDENT leaf   (baseline: call + ret)
//   _m1.._m2  single far call resident->bank (adds the generated thunk)
//   _m2.._m3  nested far call res->bankA->bankB (thunk x2, one nested)
// Interrupts are disabled across the whole measured region for determinism.
#include "types.h"
#include "far.h"

volatile u8 __at(0xE000) R[8];

// resident leaf (same body as the banked leaf) -> near-call baseline
u16 near_leaf(void) { return 7; }

void main(void)
{
	u16 a, b, c;
	__asm di __endasm;

	__asm .globl _m0
	_m0:
	__endasm;
	a = near_leaf();

	__asm .globl _m1
	_m1:
	__endasm;
	b = far_leaf();          // single far call (bank 5)

	__asm .globl _m2
	_m2:
	__endasm;
	c = far_nestA();         // nested: bank5 -> bank6

	__asm .globl _m3
	_m3:
	__endasm;

	R[0] = (u8)(a + b + c);  // keep the calls from being optimized away
	__asm ei __endasm;
	for (;;) {}
}
