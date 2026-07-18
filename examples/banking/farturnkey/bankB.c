// Bank 6 — the "scoring" bank. A leaf function in ordinary C, marked far-callable
// with FAR_FN (the macro lives in far.h and expands to nothing). It uses a runtime
// MULTIPLY (level * 100) -- SDCC emits `call __mulint`, which bankpack places ONCE in
// the resident helper pool (page 1) and shares with every bank. So banked C that uses
// * / / long "just works", with no per-bank duplication of the helpers.
#include "types.h"
#include "far.h"

// Base score for a level: 100 points per level number.
FAR_FN u16 level_score(u16 level)
{
	return level * 100;
}
