// Bank 6 — the "scoring" bank. A leaf function in ordinary C. It uses a runtime
// MULTIPLY (level * 100) -- SDCC emits `call __mulint`, which bankpack places ONCE in
// the resident helper pool (page 1) and shares with every bank. So banked C that uses
// * / / long "just works", with no per-bank duplication of the helpers.
#include "types.h"

// Base score for a level: 100 points per level number.
u16 level_score(u16 level)
{
	return level * 100;
}
