// Bank 5 — the "gameplay" bank, unchanged from test/farturnkey: what a mapper
// swap should cost the program is exactly NOTHING (one manifest line).
#include "types.h"
#include "far.h"

// Finish a level: award the level's points, plus a fixed clear bonus.
FAR_FN u16 play_level(u16 level)
{
	return level_score(level) + 50;   // scoring lives in bank 6 (bank->bank call)
}
