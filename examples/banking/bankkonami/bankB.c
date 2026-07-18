// Bank 6 — the "scoring" bank. The runtime multiply (level * 100) rides the
// shared resident helper pool (__mulint), same as on the ASCII mappers.
#include "types.h"
#include "far.h"

// Base score for a level: 100 points per level number.
FAR_FN u16 level_score(u16 level)
{
	return level * 100;
}
