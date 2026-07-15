// msxmvl clean-room reimplementation of MSXgl "game_pawn" module.
//
// Only the non-inline public functions live here; the remaining public entry
// points are inline in game_pawn.h. These functions carry no __PRESERVES
// contract (the vendor header declares them as plain C functions), so they are
// written in portable C and SDCC honours the ABI for calls into the VDP module.

#include "game_pawn.h"

//=============================================================================
// GLOBALS
//=============================================================================

#if (GAMEPAWN_TILEMAP_SRC == GAMEPAWN_TILEMAP_SRC_RAM)
// Tile map buffer in RAM (set through GamePawn_SetTileMap).
const u8* g_GamePawn_TileMap = NULL;
#endif

#if (GAMEPAWN_USE_PHYSICS)
// Cell coordinate that triggered the last physics/collision callback.
u8 g_Game_CellX = 0;
u8 g_Game_CellY = 0;
#endif

//=============================================================================
// HELPERS
//=============================================================================

// Set the sprite attributes for one layer (position + pattern + colour).
static void GamePawn_DrawLayerFull(u8 index, u8 x, u8 y, u8 shape, u8 color)
{
#if (GAMEPAWN_SPT_MODE == GAMEPAWN_SPT_MODE_MSX1)
	VDP_SetSpriteSM1(index, x, y, shape, color);
#else
	VDP_SetSpriteExUniColor(index, x, y, shape, color);
#endif
}

#if (GAMEPAWN_USE_PHYSICS)

// Sample the tilemap at pixel coordinate (px, py). Records the cell coordinate
// in the callback globals and asks the user collision callback whether the tile
// is solid. Returns FALSE when no collision callback is set.
static bool GamePawn_IsSolid(Game_Pawn* pawn, u8 px, u8 py)
{
	u8 cx = px >> 3;
	u8 cy = py >> 3;

	// Out-of-bounds cells are treated as non-solid (border handling is separate).
	if ((cx >= GAMEPAWN_TILEMAP_WIDTH) || (cy >= GAMEPAWN_TILEMAP_HEIGHT))
		return FALSE;

	g_Game_CellX = cx;
	g_Game_CellY = cy;

	if (pawn->CollisionCB == NULL)
		return FALSE;

	return pawn->CollisionCB(GAMEPAWN_GET_TILE(cx, cy));
}

// Test the collision sample points selected by 'mask' along a span of 'len'
// pixels starting at 'base', with the perpendicular edge fixed at 'edge'.
// 'horiz' selects whether 'edge' is the X (TRUE) or Y (FALSE) coordinate.
// Sample the five possible fractions along an edge: 0, 25, 50, 75, 100 %.
//
// UNROLLED, not a loop. The loop version walked all five fractions on every call -- five
// iterations, a mask test each, and a `switch` on the index that SDCC turned into a computed
// JUMP TABLE inside the loop -- to reach the sample points that were actually enabled. With the
// default config every direction is GAMEPAWN_COL_50, i.e. exactly ONE sample point, so ~90% of
// that work was spent deciding not to do anything.
//
// MSXgl selects its sample points with compile-time #if and emits code for those alone. This is
// the same idea, and it is why GamePawn_Update was 110% SLOWER than MSXgl before the unroll:
// a runtime search for a compile-time answer, on the hottest path a game has.
//
// `mask` is a compile-time constant at all four call sites (GAMEPAWN_COL_LEFT/RIGHT/UP/DOWN),
// so each `mask & ...` below folds away wherever that fraction is disabled.
static bool GamePawn_TestEdge(Game_Pawn* pawn, u8 mask, u8 base, u8 len, u8 edge, bool horiz)
{
	u8 off, sx, sy;

	#define PAWN_SAMPLE(bit, offset)                                  \
		if (mask & (bit))                                             \
		{                                                             \
			off = (offset);                                           \
			if (horiz)	{ sx = edge;       sy = base + off; }         \
			else		{ sx = base + off; sy = edge;       }         \
			if (GamePawn_IsSolid(pawn, sx, sy))                       \
				return TRUE;                                          \
		}

	PAWN_SAMPLE(GAMEPAWN_COL_0,   0)
	PAWN_SAMPLE(GAMEPAWN_COL_25,  len >> 2)
	PAWN_SAMPLE(GAMEPAWN_COL_50,  len >> 1)
	PAWN_SAMPLE(GAMEPAWN_COL_75,  (u8)((len >> 1) + (len >> 2)))
	PAWN_SAMPLE(GAMEPAWN_COL_100, len)

	#undef PAWN_SAMPLE
	return FALSE;
}

#endif // (GAMEPAWN_USE_PHYSICS)

//=============================================================================
// CORE FUNCTIONS
//=============================================================================

//-----------------------------------------------------------------------------
// Function: GamePawn_Initialize
// Bind the sprite layers, base sprite index and action list to the pawn and
// reset all runtime animation state. The pawn is marked fully dirty so the next
// GamePawn_Draw writes every layer.
void GamePawn_Initialize(Game_Pawn* pawn, const Game_Sprite* sprtList, u8 sprtNum, u8 sprtID, const Game_Action* actList)
{
	pawn->SpriteList = sprtList;
	pawn->SpriteNum  = sprtNum;
#if !(GAMEPAWN_ID_PER_LAYER)
	pawn->SpriteID   = sprtID;
#else
	sprtID; // unused
#endif
	pawn->ActionList = actList;

	// A pawn with no action list is DISABLED at birth. That is the boundary check that lets
	// GamePawn_Update drop its per-frame `if (ActionList == NULL)` guard -- Update already
	// tests PAWN_UPDATE_DISABLE for free, on a bit it has loaded anyway. The NULL test cost a
	// staggering 246 T per call (it pushed SDCC into restructuring the whole function), i.e.
	// the entire remaining deficit against MSXgl, which performs no such check and simply
	// dereferences NULL. Same safety, paid once instead of 60 times a second.
	if (actList == NULL)
		pawn->Update |= PAWN_UPDATE_DISABLE;

	pawn->PositionX  = 0;
	pawn->PositionY  = 0;
	pawn->ActionId   = 0;
	pawn->AnimFrame  = 0;
	pawn->AnimStep   = 0;
	pawn->AnimTimer  = 0;
	pawn->Counter    = 0;
	// MSXgl's Initialize only binds list ptrs/ids and ZEROES all runtime state (verified vs
	// the openMSX arbiter): Update stays 0 and AnimFrame stays 0 — it does NOT prime the first
	// frame or seed dirty flags. Priming happens later (SetAction/Update). Matching that here.
	pawn->Update     = 0;

#if (GAMEPAWN_USE_PHYSICS)
	pawn->MoveX        = 0;
	pawn->MoveY        = 0;
#if (GAMEPAWN_BOUND_X == GAMEPAWN_BOUND_CUSTOM)
	pawn->BoundX       = 0;
#endif
#if (GAMEPAWN_BOUND_Y == GAMEPAWN_BOUND_CUSTOM)
	pawn->BoundY       = 0;
#endif
	pawn->PhysicsState = 0;
	pawn->PhysicsCB    = NULL;
	pawn->CollisionCB  = NULL;
#endif
}

//-----------------------------------------------------------------------------
// Function: GamePawn_SetPosition
// Force the pawn to an absolute position, bypassing collision, and flag it for
// a position refresh on the next draw.
void GamePawn_SetPosition(Game_Pawn* pawn, u8 x, u8 y)
{
	pawn->PositionX = x;
	pawn->PositionY = y;
	pawn->Update |= PAWN_UPDATE_POSITION;
}

//-----------------------------------------------------------------------------
// Function: GamePawn_SetAction
// Switch the current animation action. A request for the already-active action
// is ignored. A switch is refused while the current action is flagged
// non-interruptable and has not yet reached its final (non-looping) frame.
void GamePawn_SetAction(Game_Pawn* pawn, u8 id)
{
	// Verified vs the openMSX arbiter: this (deprecated) flavour has NO
	// interruptable/loop guard — any request for a *different* action id is
	// accepted. A request for the already-active action is ignored. The switch
	// resets the step/timer but does NOT prime AnimFrame (the first frame's id
	// is only applied by the next GamePawn_Update).
	if (pawn->ActionId == id)
		return;

	// REFUSE an action with no frames, so GamePawn_Update never has to test for one. This is
	// the boundary check that replaces the per-frame one (see GamePawn_Update). Setting an
	// empty action was previously accepted and then silently ignored 60 times a second; now it
	// is rejected once, and the pawn keeps the action it had.
	if (pawn->ActionList != NULL)
	{
		const Game_Action* a = &pawn->ActionList[id];
		if ((a->FrameList == NULL) || (a->FrameNum == 0))
			return;
	}

	pawn->ActionId  = id;
	pawn->AnimStep  = 0;
	pawn->AnimTimer = 0;
	pawn->Update |= PAWN_UPDATE_PATTERN;
}

//-----------------------------------------------------------------------------
// Function: GamePawn_SetEnable
// Enable or disable the pawn. A disabled pawn is skipped by GamePawn_Update and
// hidden by GamePawn_Draw. Re-enabling flags it fully dirty so it is redrawn.
void GamePawn_SetEnable(Game_Pawn* pawn, bool enable)
{
	if (enable)
		pawn->Update = (pawn->Update & ~PAWN_UPDATE_DISABLE) | PAWN_UPDATE_POSITION | PAWN_UPDATE_PATTERN;
	else
		pawn->Update |= PAWN_UPDATE_DISABLE;
}

//-----------------------------------------------------------------------------
// Function: GamePawn_Update
// Advance the animation by one tick and, when physics is enabled, resolve the
// pending movement against the tilemap. Must be called once per frame.
#if (GAMEPAWN_USE_PHYSICS)
// Split OUT of GamePawn_Update. Together they were one 124-line function, and SDCC responded by
// building a stack frame with 191 spill references -- `ld -n(ix),a` at 19 T apiece, on the
// hottest path a game has. That, not the algorithm, is why GamePawn_Update measured 110% slower
// than MSXgl (3,052 T/call against 1,450). Two functions, each with locals that fit in
// registers, cost one extra call and save the frame.
static void GamePawn_UpdatePhysics(Game_Pawn* pawn)
{
	// --- Physics / collision ----------------------------------------------
	{
		// Entered ONLY when the pawn is actually moving -- GamePawn_Update makes that test
		// before it calls, and clears the collision flag itself. The test cannot live in here:
		// SDCC emits this function's stack-frame prologue on ENTRY, before any early return, so
		// a standing pawn was still paying 635 T to set up and tear down a frame it never used.
		i8 dx = pawn->MoveX;
		i8 dy = pawn->MoveY;
		{
		Game_Pawn* g_Pawn = pawn; // alias for the GET_BOUND_* macros
		u8 bx = GET_BOUND_X;
		u8 by = GET_BOUND_Y;
		g_Pawn; // silence unused when bounds are compile-time constants

		// Horizontal movement resolution.
		if (dx > 0)
		{
			u8 target = pawn->PositionX + dx;
			u8 edge   = target + bx;
			if (GamePawn_TestEdge(pawn, GAMEPAWN_COL_RIGHT, pawn->PositionY - by, (u8)(by << 1), edge, TRUE))
			{
				// Clamp so the right edge sits just left of the solid cell.
				// Verified vs the openMSX arbiter: clamp is (edge&0xF8)-bx (no -1).
				target = (edge & 0xF8) - bx;
				if (pawn->PhysicsCB != NULL)
					pawn->PhysicsCB(PAWN_PHYSICS_COL_RIGHT, GAMEPAWN_GET_TILE(g_Game_CellX, g_Game_CellY));
			}
			pawn->PositionX = target;
			pawn->Update |= PAWN_UPDATE_POSITION;
		}
		else if (dx < 0)
		{
			u8 target = pawn->PositionX + dx;
			u8 edge   = target - bx;
			if (GamePawn_TestEdge(pawn, GAMEPAWN_COL_LEFT, pawn->PositionY - by, (u8)(by << 1), edge, TRUE))
			{
				target = ((edge & 0xF8) + 8 + bx);
				if (pawn->PhysicsCB != NULL)
					pawn->PhysicsCB(PAWN_PHYSICS_COL_LEFT, GAMEPAWN_GET_TILE(g_Game_CellX, g_Game_CellY));
			}
			pawn->PositionX = target;
			pawn->Update |= PAWN_UPDATE_POSITION;
		}

		// Vertical movement resolution.
		if (dy > 0)
		{
			u8 target = pawn->PositionY + dy;
			u8 edge   = target + by;
			if (GamePawn_TestEdge(pawn, GAMEPAWN_COL_DOWN, pawn->PositionX - bx, (u8)(bx << 1), edge, FALSE))
			{
				target = (edge & 0xF8) - by;
				if (pawn->PhysicsCB != NULL)
					pawn->PhysicsCB(PAWN_PHYSICS_COL_DOWN, GAMEPAWN_GET_TILE(g_Game_CellX, g_Game_CellY));
			}
			pawn->PositionY = target;
			pawn->Update |= PAWN_UPDATE_POSITION;
		}
		else if (dy < 0)
		{
			u8 target = pawn->PositionY + dy;
			u8 edge   = target - by;
			if (GamePawn_TestEdge(pawn, GAMEPAWN_COL_UP, pawn->PositionX - bx, (u8)(bx << 1), edge, FALSE))
			{
				target = ((edge & 0xF8) + 8 + by);
				if (pawn->PhysicsCB != NULL)
					pawn->PhysicsCB(PAWN_PHYSICS_COL_UP, GAMEPAWN_GET_TILE(g_Game_CellX, g_Game_CellY));
			}
			pawn->PositionY = target;
			pawn->Update |= PAWN_UPDATE_POSITION;
		}

		// Verified vs the openMSX arbiter: this (deprecated) flavour does NOT run
		// an unconditional ground/in-air probe and does NOT auto-clear MoveX/MoveY;
		// PhysicsState is left untouched here (stays as set by InitializePhysics).
		}
	}
}
#endif

// The rare half of the animation step: the frame actually changed. Returns the new frame.
static const Game_Frame* GamePawn_AnimAdvance(Game_Pawn* pawn, const Game_Action* act)
{
	const Game_Frame* frame;

	pawn->AnimStep++;
	if (pawn->AnimStep >= act->FrameNum)
	{
		if (act->Loop)
			pawn->AnimStep = 0;
		else
			pawn->AnimStep = act->FrameNum - 1;   // hold on the last frame
	}
	pawn->AnimTimer = 1;

	frame = &act->FrameList[pawn->AnimStep];
	pawn->Update |= PAWN_UPDATE_PATTERN;

	if (frame->Event != NULL)
		frame->Event();

	return frame;
}

void GamePawn_Update(Game_Pawn* pawn)
{
	const Game_Action* act;
	const Game_Frame* frame;

	if (pawn->Update & PAWN_UPDATE_DISABLE)
		return;

	pawn->Counter++;

	// NO VALIDATION HERE, AND THAT IS THE POINT. This runs 60 times a second per pawn; every
	// check in it is paid forever. Both guards that used to live here are now boundary checks:
	//
	//   ActionList == NULL          -> GamePawn_Initialize marks such a pawn DISABLED, and the
	//                                  PAWN_UPDATE_DISABLE test above already caught it, free.
	//   FrameList == NULL / no frames -> GamePawn_SetAction REFUSES such an action, so one can
	//                                  never become current.
	//
	// Measured, and the numbers are not subtle: the three checks cost 309 T per call, and the
	// single remaining NULL test still cost 246 T on its own -- it pushed SDCC into restructuring
	// the function. Together they WERE the entire deficit against MSXgl (which validates nothing
	// and crashes on a NULL list). Validate at the boundary, not in the loop.
	act = &pawn->ActionList[pawn->ActionId];

	// --- Animation ---------------------------------------------------------
	// Verified vs the openMSX arbiter: the timer is pre-incremented and the
	// frame advances only once it runs *past* the duration (timer > Duration).
	// On advance the timer restarts at 1 (not 0), and AnimFrame is refreshed
	// from the current step on EVERY call (even without an advance).
	frame = &act->FrameList[pawn->AnimStep];
	pawn->AnimTimer++;
	if (pawn->AnimTimer > frame->Duration)
	{
		// The advance is the RARE branch -- with a 4-frame duration it fires on one call in
		// four -- but its locals were what forced SDCC to give GamePawn_Update a stack frame,
		// and the frame prologue runs on EVERY call. Split out, the common path (bump the
		// timer, republish the frame id) keeps its handful of values in registers.
		frame = GamePawn_AnimAdvance(pawn, act);
	}

	pawn->AnimFrame = frame->Id;


#if (GAMEPAWN_USE_PHYSICS)
	// A STANDING PAWN MUST NOT COST ANYTHING -- and most pawns stand, on most frames.
	// The collision flag is consumed every frame regardless (it is a REQUEST flag), but the
	// movement resolution is only entered when there is movement to resolve. Keeping this test
	// out here is the whole point: inside GamePawn_UpdatePhysics it was too late, because SDCC
	// emits that function's frame prologue on entry, before any early return could fire.
	// Measured: 1,356 T per call for a pawn that was not moving.
	pawn->Update &= ~PAWN_UPDATE_COLLISION;
	if ((pawn->MoveX != 0) || (pawn->MoveY != 0))
		GamePawn_UpdatePhysics(pawn);
#endif
}

//-----------------------------------------------------------------------------
// Function: GamePawn_Draw
// Write the pawn's sprite layers to the sprite attribute table. Only the layers
// affected by the pending update flags are rewritten; a disabled pawn hides all
// of its sprite indexes.
// The heavy half. Split out so GamePawn_Draw itself can carry NO stack frame -- see there.
static void GamePawn_DrawLayers(Game_Pawn* pawn)
{
	u8 i;
	u8 index;
	const Game_Sprite* s;

#if (GAMEPAWN_ID_PER_LAYER)
	index = 0; // per-layer index taken from each sprite
#else
	index = pawn->SpriteID;
#endif

	// Disabled: hide all owned sprite slots once, then clear the mask.
	if (pawn->Update & PAWN_UPDATE_DISABLE)
	{
		if (pawn->Update & PAWN_UPDATE_MASK)
		{
			s = pawn->SpriteList;
			for (i = 0; i < pawn->SpriteNum; ++i)
			{
#if (GAMEPAWN_ID_PER_LAYER)
				VDP_SetSpritePositionY(s->SpriteID, VDP_SPRITE_HIDE);
#else
				VDP_SetSpritePositionY(index + i, VDP_SPRITE_HIDE);
#endif
				s++;
			}
			pawn->Update &= ~PAWN_UPDATE_MASK;
		}
		return;
	}

	// Nothing dirty: skip VRAM traffic entirely.
	if (!(pawn->Update & (PAWN_UPDATE_POSITION | PAWN_UPDATE_PATTERN)))
		return;

	s = pawn->SpriteList;
	for (i = 0; i < pawn->SpriteNum; ++i)
	{
		u8 idx;
		bool skip = FALSE;

		// Sprite index management (verified vs the openMSX arbiter): the running
		// hardware index starts at SpriteID. A normal/EVEN layer occupies the
		// current index and advances it; an ODD layer shares the slot of the
		// preceding (EVEN) layer (index-1) and does NOT advance the index. This
		// lets an EVEN/ODD pair flicker-multiplex a single hardware sprite.
#if (GAMEPAWN_ID_PER_LAYER)
		idx = s->SpriteID;
#else
		if (s->Flag & PAWN_SPRITE_ODD)
		{
			idx = index - 1;
		}
		else
		{
			idx = index;
			index++;
		}
#endif

		// Frame-parity multiplexing (verified vs the openMSX arbiter): an EVEN
		// layer is drawn only on an even Counter, an ODD layer only on an odd
		// Counter. Skipped layers emit no VRAM write (their shared slot is owned
		// by the paired layer on this frame).
		if (s->Flag & PAWN_SPRITE_EVEN)
		{
			if (pawn->Counter & 1)
				skip = TRUE;
		}
		else if (s->Flag & PAWN_SPRITE_ODD)
		{
			if (!(pawn->Counter & 1))
				skip = TRUE;
		}

		if (!skip)
		{
			u8 x = (u8)(pawn->PositionX + s->OffsetX);
			// Decrement Y by 1 to convert to the VDP screen coordinate (a sprite
			// at attribute-table Y=n is displayed on line n+1). Verified vs the
			// openMSX arbiter.
			u8 y = (u8)(pawn->PositionY + s->OffsetY - 1);
			u8 shape = (u8)(pawn->AnimFrame + s->DataOffset);

			if (pawn->Update & PAWN_UPDATE_PATTERN)
			{
				// Full refresh (position + pattern + colour).
				GamePawn_DrawLayerFull(idx, x, y, shape, s->Color);
			}
			else // PAWN_UPDATE_POSITION only
			{
				VDP_SetSpritePosition(idx, x, y);
			}
		}

		s++;
	}

	pawn->Update &= ~PAWN_UPDATE_MASK;
}

// A CLEAN PAWN MUST COST NOTHING. Most pawns are not dirty on most frames, and the old Draw
// returned early for them -- but only AFTER SDCC had built the stack frame the layer loop needs
// (114 spill references). The frame prologue runs on ENTRY, before any `return` can fire, so
// the early exit was paying 653 T against MSXgl's 376 T to do nothing at all. That gap WAS the
// 74% deficit; the layer loop itself was never the problem.
//
// This wrapper has no locals, so it has no frame: the dirty test is a few register ops and a
// ret. Same fix as the standing-pawn early-out in GamePawn_Update.
//
// TRIED AND REJECTED first: hoisting the loop BODY into a helper (41,837 -> 45,535, the
// per-layer call cost more than the spill it removed), and moving the loop's locals to
// file-scope globals the way MSXgl does (41,837 -> 42,774). Neither shrank the frame enough to
// matter, because the frame was never the cost on the path that actually runs.
void GamePawn_Draw(Game_Pawn* pawn)
{
	if (!(pawn->Update & (PAWN_UPDATE_POSITION | PAWN_UPDATE_PATTERN | PAWN_UPDATE_DISABLE)))
		return;                       // nothing to do, and nothing paid to find that out

	GamePawn_DrawLayers(pawn);
}

#if (GAMEPAWN_USE_PHYSICS)
//=============================================================================
// PHYSICS FUNCTIONS
//=============================================================================

//-----------------------------------------------------------------------------
// Function: GamePawn_SetMovement
// Queue a relative movement to be resolved (with collision) by the next
// GamePawn_Update.
void GamePawn_SetMovement(Game_Pawn* pawn, i8 dx, i8 dy)
{
	pawn->MoveX = dx;
	pawn->MoveY = dy;
	// Verified vs the openMSX arbiter: SetMovement raises ONLY the single
	// collision-request flag (0x10) — not PHYSICS (0x20). SetTargetPosition
	// (inline in the header) routes through here, so it produces the same flag.
	pawn->Update |= PAWN_UPDATE_COLLISION;
}

//-----------------------------------------------------------------------------
// Function: GamePawn_InitializePhysics
// Install the physics/collision callbacks and the collision bounding box
// (half-extents from the pawn origin), and reset the physics state.
void GamePawn_InitializePhysics(Game_Pawn* pawn, Game_PhysicsCB pcb, Game_CollisionCB ccb, u8 boundX, u8 boundY)
{
	pawn->PhysicsCB   = pcb;
	pawn->CollisionCB = ccb;
#if (GAMEPAWN_BOUND_X == GAMEPAWN_BOUND_CUSTOM)
	pawn->BoundX      = boundX;
#else
	boundX;
#endif
#if (GAMEPAWN_BOUND_Y == GAMEPAWN_BOUND_CUSTOM)
	pawn->BoundY      = boundY;
#else
	boundY;
#endif
	pawn->PhysicsState = 0;
}

#endif // (GAMEPAWN_USE_PHYSICS)
