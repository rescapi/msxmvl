// msxmvl — clean-room reimplementation of MSXgl "input_manager" module public API.
// Interface (names, signatures, structs, enums, documented behaviour) taken from
// MSXgl's engine/src/input_manager.h header ONLY. No MSXgl implementation source
// (.c/.asm/.lst) was read. Drop-in: exposes MSXgl's exact public names + signatures
// so one difftest driver can link against real MSXgl OR msxmvl.
#ifndef MSXMVL_INPUT_MANAGER_H
#define MSXMVL_INPUT_MANAGER_H

#include "types.h"

//=============================================================================
// INPUT (subset of MSXgl input.h needed by the manager + the driver)
// These mirror the public defines/macros from MSXgl engine/src/input.h.
// Guarded so they don't clash if the real input.h is also in scope.
//=============================================================================

#ifndef JOY_PORT_1
#define JOY_PORT_1					0b00001111
#define JOY_PORT_2					0b01001111
#endif

#ifndef JOY_INPUT_DIR_NONE
#define JOY_INPUT_DIR_NONE			0
#define JOY_INPUT_DIR_UP			(1 << 0)
#define JOY_INPUT_DIR_DOWN			(1 << 1)
#define JOY_INPUT_DIR_LEFT			(1 << 2)
#define JOY_INPUT_DIR_RIGHT			(1 << 3)
#define JOY_INPUT_DIR_MASK			0x0F
#define JOY_INPUT_TRIGGER_A			(1 << 4)
#define JOY_INPUT_TRIGGER_B			(1 << 5)
#endif

#ifndef MAKE_KEY
#define MAKE_KEY(r, b)		(((b) << 4) | (r))
#define KEY_ROW(key)		((key) & 0x0F)
#define KEY_IDX(key)		((key) >> 4)
#define KEY_FLAG(key)		(1 << KEY_IDX(key))
#endif

// Minimal KEY_ID subset used by the default config's two key-sets (cursor+Space/N and
// WASD+Ctrl/Shift). When compiled against full input.h these come from there instead.
#ifndef KEY_SPACE
#define KEY_N			MAKE_KEY(4, 3)
#define KEY_SPACE		MAKE_KEY(8, 0)
#define KEY_LEFT		MAKE_KEY(8, 4)
#define KEY_UP			MAKE_KEY(8, 5)
#define KEY_DOWN		MAKE_KEY(8, 6)
#define KEY_RIGHT		MAKE_KEY(8, 7)
#endif
#ifndef KEY_W
#define KEY_A			MAKE_KEY(2, 6)
#define KEY_D			MAKE_KEY(3, 1)
#define KEY_S			MAKE_KEY(5, 0)
#define KEY_W			MAKE_KEY(5, 4)
#define KEY_SHIFT		MAKE_KEY(6, 0)
#define KEY_CTRL		MAKE_KEY(6, 1)
#endif

//=============================================================================
// DEFINES
//=============================================================================

// Device ID
enum IPM_DEVICE
{
	IPM_DEVICE_JOYSTICK_1 = 0,
	IPM_DEVICE_JOYSTICK_2,
	IPM_DEVICE_KEYBOARD_1,
	IPM_DEVICE_KEYBOARD_2,
	//---------------------------------
	IPM_DEVICE_MAX,
	IPM_DEVICE_ANY,
};

// Device input ID
enum IPM_INPUT
{
	IPM_INPUT_STICK = 0,
	IPM_INPUT_BUTTON_A,
	IPM_INPUT_BUTTON_B,
	//---------------------------------
	IPM_INPUT_MAX,
	IPM_INPUT_ANY,
};

// Device event ID
enum IPM_EVENT
{
	IPM_EVENT_CLICK = 0,
	IPM_EVENT_HOLD,
	IPM_EVENT_DOUBLE_CLICK,
	IPM_EVENT_DOUBLE_CLICK_HOLD,
	IPM_EVENT_RELEASE,
	//---------------------------------
	IPM_EVENT_MAX,
	IPM_EVENT_ANY,
	IPM_EVENT_NONE,
};

// State flag
#define IPM_STATE_OFF			0x00
#define IPM_STATE_PRESS			0x01
#define IPM_STATE_ON			0x02
#define IPM_STATE_RELEASE		0x03
#define IPM_STATE_PRESSMASK		0x03
#define IPM_STATE_HOLD			0x10
#define IPM_STATE_HOLDING		0x20
#define IPM_STATE_HOLDMASK		0x30
#define IPM_STATE_DOUBLE		0x80

#define IPM_EVENT_TAB_SIZE		16

// Callback function prototypes
typedef void (*IPM_cb)(u8 joy, u8 in, u8 evt);
typedef u8 (*IPM_check)(u8 joy, u8 in);

// IPM event structure
typedef struct IPM_Event
{
	u8			Device;
	u8			Input;
	u8			Event;
	IPM_cb		Callback;
} IPM_Event;

// IPM process structure
typedef struct IPM_Process
{
	u8			CurrentStatus;
	u8			PreviousStatus;
	u8			State[IPM_INPUT_MAX];
	u8			Timer[IPM_INPUT_MAX];
} IPM_Process;

// IPM key set structure
typedef struct IPM_KeySet
{
	u8			Up;
	u8			Right;
	u8			Down;
	u8			Left;
	u8			TriggerA;
	u8			TriggerB;
} IPM_KeySet;

// IPM configuration structure
typedef struct IPM_Config
{
	u8			DeviceSupport[IPM_DEVICE_MAX];
	u8			HoldTimer;
	u8			DoubleClickTimer;
	IPM_KeySet	KeySet[2];
} IPM_Config;

// IPM internal data structure
typedef struct IPM_Data
{
	IPM_Process	Process[IPM_DEVICE_MAX];
	IPM_Event	Events[IPM_EVENT_TAB_SIZE];
	u8			EventsNum;
	IPM_check	Checker[IPM_EVENT_MAX];
	IPM_Config  Config;
} IPM_Data;

// Manager internal variables
extern IPM_Data g_IPM;

//=============================================================================
// FUNCTIONS
//=============================================================================

// Function: IPM_Initialize
// Initialize input manager
//
// Parameters:
//   config - Input manager configuration structure
void IPM_Initialize(IPM_Config* config);

// Function: IPM_SetTimer
// Set double-click and hold timers
//
// Parameters:
//   doubleClk - Double click timer
//   hold - Hold timer
inline void IPM_SetTimer(u8 doubleClk, u8 hold)
{
	g_IPM.Config.DoubleClickTimer = doubleClk;
	g_IPM.Config.HoldTimer = hold;
}

// Function: IPM_Update
// Update device manager
void IPM_Update();

// Function: IPM_RegisterEvent
// Register a callback to a given device manager's event
//
// Parameters:
//   dev   - Device ID
//   input - Input ID
//   event - Event ID
//   cb    - Callback function
//
// Return:
//   TRUE if the event was registered, FALSE otherwise
bool IPM_RegisterEvent(u8 dev, u8 input, u8 event, IPM_cb cb);

// Function: IPM_GetStatus
// Get current device status
//
// Parameters:
//   dev - Device ID
//
// Return:
//   Current device status
inline u8 IPM_GetStatus(u8 dev) { return g_IPM.Process[dev].CurrentStatus; }

// Function: IPM_GetStickDirection
// Get current direction of the given device
//
// Parameters:
//   dev - Device ID
//
// Return:
//   Current device direction
u8 IPM_GetStickDirection(u8 dev);

// Function: IPM_GetInputState
// Get current device state
//
// Parameters:
//   dev   - Device ID
//   input - Input ID
//
// Return:
//   Current device's input state
inline u8 IPM_GetInputState(u8 dev, u8 input) { return g_IPM.Process[dev].State[input]; }

// Function: IPM_GetInputTimer
// Get current device state timer
//
// Parameters:
//   dev   - Device ID
//   input - Input ID
//
// Return:
//   Current device's input state timer
inline u8 IPM_GetInputTimer(u8 dev, u8 input) { return g_IPM.Process[dev].Timer[input]; }

// Function: IPM_GetEventName
// Get event name
//
// Parameters:
//   ev - Event ID
//
// Return:
//   Zero-terminated string containing the event name
const c8* IPM_GetEventName(u8 ev);

#endif // MSXMVL_INPUT_MANAGER_H
