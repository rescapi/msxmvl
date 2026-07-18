// Bank 5. far_leaf: trivial leaf (same body as resident near_leaf) for the
// single far-call measurement. nestA: calls far_nestB in bank 6 (nested case).
#include "types.h"
#include "far.h"

u16 leaf(void) { return 7; }

u16 nestA(void) { return far_nestB(); }
