// Bank 5 — the "gameplay" bank. Ordinary C: FAR_FN marks the definition as
// far-callable (bankpack's gen scan reads the annotation and derives the thunk +
// dispatch slot; to the compiler the macro expands to nothing). It reaches bank 6's
// level_score by its plain name too: a bank->bank call, routed automatically through
// the resident dispatch table. A whole subsystem (menus, level logic, an ending) can
// live in its own ROM bank like this and be reached from anywhere.
#include "types.h"
#include "far.h"

// Finish a level: award the level's points, plus a fixed clear bonus.
FAR_FN u16 play_level(u16 level)
{
	return level_score(level) + 50;   // 50-point clear bonus; scoring lives in bank 6
}
