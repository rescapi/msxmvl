// msxmvl clean-room reimplementation of MSXgl "game_pawn" module.
// Drop-in: exposes MSXgl's exact public names + signatures so one test driver
// can link against real MSXgl or msxmvl.
//
// Game pawn module: animated multi-layer sprite entity with optional
// tilemap-based physics/collision.
//
// The vendor "game_pawn.h" is the (deprecated) self-contained flavour of the
// module. It declares its public functions as plain C functions (no
// __PRESERVES contract), so the bodies in game_pawn.c are portable C and the
// SDCC ABI is honoured for the calls into the VDP module.
#ifndef MSXMVL_GAME_PAWN_H
#define MSXMVL_GAME_PAWN_H

#include "types.h"

#ifndef MSXMVL_CALLBACK_DEFINED
#define MSXMVL_CALLBACK_DEFINED
typedef void (*callback)(void);			// Callback default signature
#endif

//=============================================================================
// CONFIG OPTION VALUES (mirror of MSXgl config_option.h)
//=============================================================================

// Collision position options
#define PAWN_COL_NONE				0
#define PAWN_COL_0					1
#define PAWN_COL_25					2
#define PAWN_COL_50					4
#define PAWN_COL_75					8
#define PAWN_COL_100				16

#define GAMEPAWN_COL_NONE			PAWN_COL_NONE
#define GAMEPAWN_COL_0				PAWN_COL_0
#define GAMEPAWN_COL_25				PAWN_COL_25
#define GAMEPAWN_COL_50				PAWN_COL_50
#define GAMEPAWN_COL_75				PAWN_COL_75
#define GAMEPAWN_COL_100			PAWN_COL_100

#define PAWN_BOUND_CUSTOM			0x10000
#define GAMEPAWN_BOUND_CUSTOM		PAWN_BOUND_CUSTOM

// Collision tilemap source
#define PAWN_TILEMAP_SRC_AUTO		0
#define PAWN_TILEMAP_SRC_RAM		1
#define PAWN_TILEMAP_SRC_VRAM		2
#define PAWN_TILEMAP_SRC_V9			3
#define GAMEPAWN_TILEMAP_SRC_AUTO	PAWN_TILEMAP_SRC_AUTO
#define GAMEPAWN_TILEMAP_SRC_RAM	PAWN_TILEMAP_SRC_RAM
#define GAMEPAWN_TILEMAP_SRC_VRAM	PAWN_TILEMAP_SRC_VRAM
#define GAMEPAWN_TILEMAP_SRC_V9		PAWN_TILEMAP_SRC_V9

// Pawn's sprite mode
#define PAWN_SPT_MODE_AUTO			0x00
#define PAWN_SPT_MODE_MSX1			0x10
#define PAWN_SPT_MODE_MSX2			0x20
#define GAMEPAWN_SPT_MODE_AUTO		PAWN_SPT_MODE_AUTO
#define GAMEPAWN_SPT_MODE_MSX1		PAWN_SPT_MODE_MSX1
#define GAMEPAWN_SPT_MODE_MSX2		PAWN_SPT_MODE_MSX2

//=============================================================================
// OPTIONS (msxmvl default configuration)
//=============================================================================

#ifndef GAMEPAWN_ID_PER_LAYER
	#define GAMEPAWN_ID_PER_LAYER		FALSE
#endif
#ifndef GAMEPAWN_USE_PHYSICS
	#define GAMEPAWN_USE_PHYSICS		TRUE
#endif
#ifndef GAMEPAWN_BORDER_EVENT
	#define GAMEPAWN_BORDER_EVENT		0
#endif
#ifndef GAMEPAWN_BORDER_BLOCK
	#define GAMEPAWN_BORDER_BLOCK		0
#endif
#ifndef GAMEPAWN_BORDER_MIN_Y
	#define GAMEPAWN_BORDER_MIN_Y		0
#endif
#ifndef GAMEPAWN_BORDER_MAX_Y
	#define GAMEPAWN_BORDER_MAX_Y		192
#endif
#ifndef GAMEPAWN_COL_DOWN
	#define GAMEPAWN_COL_DOWN			GAMEPAWN_COL_50
#endif
#ifndef GAMEPAWN_COL_UP
	#define GAMEPAWN_COL_UP				GAMEPAWN_COL_50
#endif
#ifndef GAMEPAWN_COL_RIGHT
	#define GAMEPAWN_COL_RIGHT			GAMEPAWN_COL_50
#endif
#ifndef GAMEPAWN_COL_LEFT
	#define GAMEPAWN_COL_LEFT			GAMEPAWN_COL_50
#endif
#ifndef GAMEPAWN_BOUND_X
	#define GAMEPAWN_BOUND_X			GAMEPAWN_BOUND_CUSTOM
#endif
#ifndef GAMEPAWN_BOUND_Y
	#define GAMEPAWN_BOUND_Y			GAMEPAWN_BOUND_CUSTOM
#endif
#ifndef GAMEPAWN_TILEMAP_WIDTH
	#define GAMEPAWN_TILEMAP_WIDTH		32
#endif
#ifndef GAMEPAWN_TILEMAP_HEIGHT
	#define GAMEPAWN_TILEMAP_HEIGHT		24
#endif
#ifndef GAMEPAWN_TILEMAP_SRC
	#define GAMEPAWN_TILEMAP_SRC		GAMEPAWN_TILEMAP_SRC_RAM
#endif
#ifndef GAMEPAWN_SPT_MODE
	#define GAMEPAWN_SPT_MODE			GAMEPAWN_SPT_MODE_MSX2
#endif

//=============================================================================
// DEFINES
//=============================================================================

// Animation frame structure (one pose of the pawn)
typedef struct Game_Frame
{
	u8					Id;       // Animation frame data index (0-255)
	u8					Duration; // Frame duration (in frame number)
	callback			Event;    // Event to execute at this frame
} Game_Frame;

// Animation action structure
typedef struct Game_Action
{
	const Game_Frame*	FrameList;     // Animation frames data
	u8					FrameNum;      // Animation frames data count
	u8					Loop      : 1; // Is action looping?
	u8					Interrupt : 1; // Is action interruptable?
} Game_Action;

// Pawn sprite flags
enum PAWN_SPRITE_FLAG
{
	PAWN_SPRITE_EVEN = 0b00000001,
	PAWN_SPRITE_ODD  = 0b00000010,
	PAWN_SPRITE_OR   = 0b00000100,
};

// Pawn structure
typedef struct Game_Sprite
{
#if (GAMEPAWN_ID_PER_LAYER)
	u8					SpriteID;     // Sprite ID
#endif
	i8					OffsetX;      // Layer position offset...
	i8					OffsetY;      // ...can be positive or negative
	u8					DataOffset;   // Data index offset
	u8					Color;        // Sprite color
	u8					Flag;         // Sprite flag
} Game_Sprite;

// Pawn update flags
enum PAWN_UPDATE_FLAG
{
	PAWN_UPDATE_POSITION  = 0b00000001,
	PAWN_UPDATE_PATTERN   = 0b00000010,
	PAWN_UPDATE_2         = 0b00000100,
	PAWN_UPDATE_3         = 0b00001000,

	PAWN_UPDATE_MASK	  = 0b00001111,

	#if (GAMEPAWN_USE_PHYSICS)
	PAWN_UPDATE_COLLISION = 0b00010000,
	PAWN_UPDATE_PHYSICS   = 0b00100000,
	#endif
	PAWN_UPDATE_7         = 0b01000000,
	PAWN_UPDATE_DISABLE   = 0b10000000,
};

#if (GAMEPAWN_USE_PHYSICS)
// Physics callback
typedef void (*Game_PhysicsCB)(u8 event, u8 tile);

// Collision callback
typedef bool (*Game_CollisionCB)(u8 tile);

// Pawn physics events
enum PAWN_PHYSICS_EVENT
{
	// Collision
	PAWN_PHYSICS_COL_DOWN  = 0,
	PAWN_PHYSICS_COL_UP,
	PAWN_PHYSICS_COL_RIGHT,
	PAWN_PHYSICS_COL_LEFT,
	// Physics
	PAWN_PHYSICS_FALL,
	// Border
	#if (GAMEPAWN_BORDER_EVENT || GAMEPAWN_BORDER_BLOCK)
		PAWN_PHYSICS_BORDER_DOWN,
		PAWN_PHYSICS_BORDER_UP,
		PAWN_PHYSICS_BORDER_RIGHT,
		PAWN_PHYSICS_BORDER_LEFT,
	#endif
};

// Pawn physics states
enum PAWN_PHYSICS_STATE
{
	PAWN_PHYSICS_INAIR     = 0b00000001, // Grounded overwise
};
#endif

// Pawn structure
typedef struct Game_Pawn
{
	const Game_Sprite*	SpriteList;		// List of sprite layers
	u8					SpriteNum;		// Number of sprite layers
#if !(GAMEPAWN_ID_PER_LAYER)
	u8					SpriteID;		// Sprite ID of the first layer (0~31)
#endif
	const Game_Action*	ActionList;		// List of actions
	u8					PositionX;		// Pawn screen position
	u8					PositionY;
	u8					ActionId;		// Current action id
	u8					AnimFrame;		// Current animation frame id
	u8					AnimStep;		// Current step into the animation
	u8					AnimTimer;		// Animation timer (into the current step)
	u8					Update;			// Pawn update flags
	u8					Counter;		// Global update counter
#if (GAMEPAWN_USE_PHYSICS)
	i8					MoveX;			// Pawn movement offset
	i8					MoveY;
#if (GAMEPAWN_BOUND_X == GAMEPAWN_BOUND_CUSTOM)
	u8					BoundX;			// Pawn collision bound
#endif
#if (GAMEPAWN_BOUND_Y == GAMEPAWN_BOUND_CUSTOM)
	u8					BoundY;
#endif
	u8					PhysicsState;
	Game_PhysicsCB		PhysicsCB;
	Game_CollisionCB	CollisionCB;
#endif
} Game_Pawn;

#if (GAMEPAWN_BOUND_X == GAMEPAWN_BOUND_CUSTOM)
	#define GET_BOUND_X		g_Pawn->BoundX
#else
	#define GET_BOUND_X		GAMEPAWN_BOUND_X
#endif

#if (GAMEPAWN_BOUND_Y == GAMEPAWN_BOUND_CUSTOM)
	#define GET_BOUND_Y		g_Pawn->BoundY
#else
	#define GET_BOUND_Y		GAMEPAWN_BOUND_Y
#endif

// Tile map getter macro
#if (GAMEPAWN_TILEMAP_SRC == GAMEPAWN_TILEMAP_SRC_RAM)
	#define GAMEPAWN_GET_TILE(X, Y)	g_GamePawn_TileMap[((Y) * GAMEPAWN_TILEMAP_WIDTH) + (X)]
	// Tile map buffer in RAM
	extern const u8* g_GamePawn_TileMap;
#endif

#if (GAMEPAWN_USE_PHYSICS)
// Current cell coordinate
extern u8 g_Game_CellX;
extern u8 g_Game_CellY;
#endif

//=============================================================================
// VDP MODULE DECLARATIONS (provided by the VDP module / real MSXgl at link)
//=============================================================================
#ifndef MSXMVL_GAME_PAWN_VDP_DECLS
#define MSXMVL_GAME_PAWN_VDP_DECLS
	extern u16 g_SpriteAttributeLow;
	// Sprite Mode 1 attribute setter.
	void VDP_SetSpriteSM1(u8 index, u8 x, u8 y, u8 shape, u8 color);
	// Sprite Mode 2 unicolor attribute setter (fills the sprite colour table).
	void VDP_SetSpriteExUniColor(u8 index, u8 x, u8 y, u8 shape, u8 color);
	// Update sprite position only.
	void VDP_SetSpritePosition(u8 index, u8 x, u8 y);
	// Update sprite pattern only.
	void VDP_SetSpritePattern(u8 index, u8 shape);
	// Update sprite Y position only (used to hide sprites).
	void VDP_SetSpritePositionY(u8 index, u8 y);
	#define VDP_SPRITE_HIDE		213
#endif // MSXMVL_GAME_PAWN_VDP_DECLS

//=============================================================================
// FUNCTIONS
//=============================================================================
// Group: Core

// Function: GamePawn_Initialize
// Initialize a game pawn.
void GamePawn_Initialize(Game_Pawn* pawn, const Game_Sprite* sprtList, u8 sprtNum, u8 sprtID, const Game_Action* actList);

// Function: GamePawn_SetPosition
// Set game pawn position. Force movement even if collision is activated.
void GamePawn_SetPosition(Game_Pawn* pawn, u8 x, u8 y);

// Function: GamePawn_SetAction
// Set game pawn current action index.
void GamePawn_SetAction(Game_Pawn* pawn, u8 id);

#if (GAMEPAWN_TILEMAP_SRC == GAMEPAWN_TILEMAP_SRC_RAM)
// Function: GamePawn_SetTileMap
// Set the tile map to be used for collision.
inline void GamePawn_SetTileMap(const u8* map) { g_GamePawn_TileMap = map; }
#endif

// Function: GamePawn_SetEnable
// Set if pawn is enable or disable.
void GamePawn_SetEnable(Game_Pawn* pawn, bool enable);

// Function: GamePawn_Disable
// Disable a pawn.
inline void GamePawn_Disable(Game_Pawn* pawn) { GamePawn_SetEnable(pawn, FALSE); }

// Function: GamePawn_Enable
// Enable a pawn.
inline void GamePawn_Enable(Game_Pawn* pawn) { GamePawn_SetEnable(pawn, TRUE); }

// Function: GamePawn_Update
// Update animation (and physics) of the game pawn. Must be called once a frame.
void GamePawn_Update(Game_Pawn* pawn);

// Function: GamePawn_Draw
// Update rendering of the game pawn. Must be called once a frame.
void GamePawn_Draw(Game_Pawn* pawn);

#if (GAMEPAWN_USE_PHYSICS)
// Group: Physics

// Function: GamePawn_SetMovement
// Set pawn relative movement. Collision can prevent part of the movement.
void GamePawn_SetMovement(Game_Pawn* pawn, i8 dx, i8 dy);

// Function: GamePawn_SetTargetPosition
// Set pawn absolute movement. Collision can prevent part of the movement.
inline void GamePawn_SetTargetPosition(Game_Pawn* pawn, u8 x, u8 y) { GamePawn_SetMovement(pawn, x - pawn->PositionX, y - pawn->PositionY); }

// Function: GamePawn_InitializePhysics
// Set pawn physics/collision callbacks and bounding box.
void GamePawn_InitializePhysics(Game_Pawn* pawn, Game_PhysicsCB pcb, Game_CollisionCB ccb, u8 boundX, u8 boundY);

// Function: GamePawn_GetPhysicsState
// Get pawn physics state.
inline u8 GamePawn_GetPhysicsState(Game_Pawn* pawn) { return pawn->PhysicsState; }

// Function: GamePawn_GetCallbackCellX
// Get the X coordinate of the cell that triggered a callback.
inline u8 GamePawn_GetCallbackCellX() { return g_Game_CellX; }

// Function: GamePawn_GetCallbackCellY
// Get the Y coordinate of the cell that triggered a callback.
inline u8 GamePawn_GetCallbackCellY() { return g_Game_CellY; }

#endif // (GAMEPAWN_USE_PHYSICS)

#endif // MSXMVL_GAME_PAWN_H
