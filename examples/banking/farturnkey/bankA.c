// Bank 5 — the "gameplay" bank. Ordinary C. It calls a function that lives in bank 6
// using the same far_ syntax: a bank->bank call, routed automatically through the
// resident dispatch table. A whole subsystem (menus, level logic, an ending) can live
// in its own ROM bank like this and be reached from anywhere.
#include "types.h"
#include "far.h"

// Finish a level: award the level's points, plus a fixed clear bonus.
u16 play_level(u16 level)
{
	return far_level_score(level) + 50;   // 50-point clear bonus; scoring lives in bank 6
}
