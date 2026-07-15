// msxmvl — clean-room reimplementation of MSXgl "input_manager" module.
// Advanced input manager: samples joystick/keyboard devices each tick, derives
// per-input press/hold/double-click state, and dispatches registered callbacks.
//
// Implemented from the public contract in engine/src/input_manager.h + standard
// MSX PSG/PPI I/O knowledge. No MSXgl implementation body was consulted.
#include "input_manager.h"

//=============================================================================
// GLOBALS
//=============================================================================

IPM_Data g_IPM;

// Built-in default configuration, applied when IPM_Initialize() is called with NULL.
// Mirrors MSXgl's g_DefaultConfig: all four devices enabled, hold=0x10, double-click=0x04,
// cursor+Space/N as keyset 0 and WASD+Ctrl/Shift as keyset 1.
const IPM_Config g_IPM_DefaultConfig =
{
	{ TRUE, TRUE, TRUE, TRUE },			// DeviceSupport
	0x10, 0x04,							// HoldTimer, DoubleClickTimer
	{
		{ KEY_UP, KEY_RIGHT, KEY_DOWN, KEY_LEFT, KEY_SPACE, KEY_N },
		{ KEY_W,  KEY_D,     KEY_S,    KEY_A,    KEY_CTRL,  KEY_SHIFT },
	}
};

//=============================================================================
// LOW-LEVEL DEVICE READ (self-contained; mirrors input.h semantics)
//=============================================================================

// Read a joystick port. 'port' is JOY_PORT_1 (0x0F) or JOY_PORT_2 (0x4F);
// bit 6 selects the port. Returns raw R#14 value: bit=0 means pressed, xxBARLDU.
static u8 IPM_ReadJoy(u8 port) __naked
{
	port; // arg arrives in A (--sdcccall 1, 8-bit)
	__asm
		and  #0x40				; isolate the port-select bit (bit6)
		ld   c, a				; C = port select
		di
		ld   a, #15				; address PSG R#15 (I/O port B, output)
		out  (#0xA0), a
		in   a, (#0xA2)			; read current R#15
		and  #0b10001111		; keep bit7 + low nibble, clear pin-8 / port bits
		or   c					; apply port select (bit6)
		or   #0b00110000		; drive pin-8 outputs high (bits 4,5)
		out  (#0xA1), a			; write back R#15
		ld   a, #14				; address PSG R#14 (I/O port A, joystick input)
		out  (#0xA0), a
		in   a, (#0xA2)			; A = joystick bits (active low)
		ei
		ret
	__endasm;
}

// Read one keyboard matrix row (0-10). Returns 8 bits, bit=0 means key pressed.
static u8 IPM_ReadKbRow(u8 row) __naked
{
	row; // arg arrives in A
	__asm
		ld   c, a				; C = requested row
		di
		in   a, (#0xAA)			; PPI port C
		and  #0xF0				; keep high nibble (caps/click/etc.)
		or   c					; low nibble = row to scan
		out  (#0xAA), a
		in   a, (#0xA9)			; PPI port B = key states (active low)
		ei
		ret
	__endasm;
}

// Is the given key (MAKE_KEY encoded) currently pressed?
static bool IPM_KeyPressed(u8 key)
{
	return (IPM_ReadKbRow(KEY_ROW(key)) & KEY_FLAG(key)) == 0;
}

// Build a joystick-format status byte (xxBARLDU, active low) from a key set.
static u8 IPM_ReadKeyboard(IPM_KeySet* ks)
{
	u8 st = 0xFF; // all released
	if (IPM_KeyPressed(ks->Up))       st &= (u8)~JOY_INPUT_DIR_UP;
	if (IPM_KeyPressed(ks->Down))     st &= (u8)~JOY_INPUT_DIR_DOWN;
	if (IPM_KeyPressed(ks->Left))     st &= (u8)~JOY_INPUT_DIR_LEFT;
	if (IPM_KeyPressed(ks->Right))    st &= (u8)~JOY_INPUT_DIR_RIGHT;
	if (IPM_KeyPressed(ks->TriggerA)) st &= (u8)~JOY_INPUT_TRIGGER_A;
	if (IPM_KeyPressed(ks->TriggerB)) st &= (u8)~JOY_INPUT_TRIGGER_B;
	return st;
}

//=============================================================================
// STATE HELPERS
//=============================================================================

// Is the given input active in a joystick-format status byte (active low)?
// A MACRO, not a static function. SDCC will not inline a static this small, so this cost a
// real `call` for EVERY (device, input) pair on EVERY frame -- and IPM_Update is called once
// per frame, forever. Same lesson as Print_BppOfMode. (Args are plain locals at both call
// sites, so the double evaluation below is free.)
#define IPM_IsActive(status, input)                                       \
	(((input) == IPM_INPUT_STICK)                                         \
		? (((u8)~(status) & JOY_INPUT_DIR_MASK) != 0)                     \
		: (((input) == IPM_INPUT_BUTTON_A)                                \
			? (((status) & JOY_INPUT_TRIGGER_A) == 0)                     \
			: (((status) & JOY_INPUT_TRIGGER_B) == 0)))

//=============================================================================
// EVENT CHECKERS (one per IPM_EVENT_*; stored in g_IPM.Checker[])
//=============================================================================

// A single click: press edge without an active double flag.
static u8 IPM_CheckClick(u8 dev, u8 in)
{
	u8 st = g_IPM.Process[dev].State[in];
	return ((st & IPM_STATE_PRESSMASK) == IPM_STATE_PRESS) && ((st & IPM_STATE_DOUBLE) == 0);
}

// Hold trigger frame (single press).
static u8 IPM_CheckHold(u8 dev, u8 in)
{
	u8 st = g_IPM.Process[dev].State[in];
	return ((st & IPM_STATE_HOLDMASK) == IPM_STATE_HOLD) && ((st & IPM_STATE_DOUBLE) == 0);
}

// Double click: press edge with the double flag set.
static u8 IPM_CheckDoubleClick(u8 dev, u8 in)
{
	u8 st = g_IPM.Process[dev].State[in];
	return ((st & IPM_STATE_PRESSMASK) == IPM_STATE_PRESS) && ((st & IPM_STATE_DOUBLE) != 0);
}

// Double click then hold.
static u8 IPM_CheckDoubleClickHold(u8 dev, u8 in)
{
	u8 st = g_IPM.Process[dev].State[in];
	return ((st & IPM_STATE_HOLDMASK) == IPM_STATE_HOLD) && ((st & IPM_STATE_DOUBLE) != 0);
}

// Release edge.
static u8 IPM_CheckRelease(u8 dev, u8 in)
{
	u8 st = g_IPM.Process[dev].State[in];
	return (st & IPM_STATE_PRESSMASK) == IPM_STATE_RELEASE;
}

//=============================================================================
// EVENT NAMES
//=============================================================================

static const c8* const g_IPM_EventName[IPM_EVENT_MAX] =
{
	"Clck",
	"Hold",
	"DClk",
	"DCHo",
	"Rel.",
};

//=============================================================================
// PUBLIC FUNCTIONS
//=============================================================================

//-----------------------------------------------------------------------------
// Function: IPM_Initialize
// Copy the supplied configuration, clear all per-device process state and the
// event table, and install the internal per-event checker functions.
void IPM_Initialize(IPM_Config* config)
{
	u8 i, j;

	// A NULL config selects the built-in defaults, as MSXgl does; without this a NULL
	// argument would LDIR from address 0 (BIOS ROM) into the config.
	if (config == NULL)
		config = (IPM_Config*)&g_IPM_DefaultConfig;

	// Copy configuration structure (LDIR instead of a byte-by-byte C loop).
	__builtin_memcpy((u8*)&g_IPM.Config, (const u8*)config, sizeof(IPM_Config));

	// Reset process state for every device (idle = nothing pressed).
	for (i = 0; i < IPM_DEVICE_MAX; i++)
	{
		g_IPM.Process[i].CurrentStatus  = 0xFF;
		g_IPM.Process[i].PreviousStatus = 0xFF;
		for (j = 0; j < IPM_INPUT_MAX; j++)
		{
			g_IPM.Process[i].State[j] = IPM_STATE_OFF;
			g_IPM.Process[i].Timer[j] = 0xFF;
		}
	}

	// Empty the event table.
	g_IPM.EventsNum = 0;

	// Install checkers.
	g_IPM.Checker[IPM_EVENT_CLICK]             = IPM_CheckClick;
	g_IPM.Checker[IPM_EVENT_HOLD]              = IPM_CheckHold;
	g_IPM.Checker[IPM_EVENT_DOUBLE_CLICK]      = IPM_CheckDoubleClick;
	g_IPM.Checker[IPM_EVENT_DOUBLE_CLICK_HOLD] = IPM_CheckDoubleClickHold;
	g_IPM.Checker[IPM_EVENT_RELEASE]           = IPM_CheckRelease;
}

//-----------------------------------------------------------------------------
// Advance the press/hold/double-click state machine for one input of one device.
// Takes the PROCESS POINTER, not the device index. It used to take `dev` and redo
// `&g_IPM.Process[dev]` -- a struct-index multiply -- on every single input, although the only
// caller (IPM_UpdateDevice) already had the pointer in hand.
static void IPM_UpdateInput(IPM_Process* p, u8 in)
{
	u8 st    = p->State[in];
	u8 press = st & IPM_STATE_PRESSMASK;
	bool cur = IPM_IsActive(p->CurrentStatus, in);

	if (cur)
	{
		if ((press == IPM_STATE_OFF) || (press == IPM_STATE_RELEASE))
		{
			// Rising edge -> PRESS. Double if we are still inside the window.
			u8 ns = IPM_STATE_PRESS;
			if (p->Timer[in] > 0)
				ns |= IPM_STATE_DOUBLE;
			p->State[in] = ns;
			p->Timer[in] = 0;
		}
		else
		{
			// Held down -> ON, carrying any double flag; drive the hold timer.
			u8 dbl  = st & IPM_STATE_DOUBLE;
			u8 hold = 0;
			if (p->Timer[in] < g_IPM.Config.HoldTimer)
			{
				p->Timer[in]++;
				if (p->Timer[in] == g_IPM.Config.HoldTimer)
					hold = IPM_STATE_HOLD;		// trigger frame
			}
			else
			{
				hold = IPM_STATE_HOLDING;		// sustained
			}
			p->State[in] = IPM_STATE_ON | dbl | hold;
		}
	}
	else
	{
		if ((press == IPM_STATE_PRESS) || (press == IPM_STATE_ON))
		{
			// Falling edge -> RELEASE. Open the double-click window.
			u8 dbl = st & IPM_STATE_DOUBLE;
			p->State[in] = IPM_STATE_RELEASE | dbl;
			p->Timer[in] = g_IPM.Config.DoubleClickTimer;
		}
		else
		{
			// Idle: count the double-click window down.
			if (p->Timer[in] > 0)
				p->Timer[in]--;
			p->State[in] = IPM_STATE_OFF;
		}
	}
}

//-----------------------------------------------------------------------------
// Sample a single device and refresh its status + per-input state.
static void IPM_UpdateDevice(u8 dev)
{
	IPM_Process* p = &g_IPM.Process[dev];
	u8 status;
	u8 in;

	p->PreviousStatus = p->CurrentStatus;

	switch (dev)
	{
	case IPM_DEVICE_JOYSTICK_1: status = IPM_ReadJoy(JOY_PORT_1); break;
	case IPM_DEVICE_JOYSTICK_2: status = IPM_ReadJoy(JOY_PORT_2); break;
	case IPM_DEVICE_KEYBOARD_1: status = IPM_ReadKeyboard(&g_IPM.Config.KeySet[0]); break;
	case IPM_DEVICE_KEYBOARD_2: status = IPM_ReadKeyboard(&g_IPM.Config.KeySet[1]); break;
	default:                    status = 0xFF; break;
	}

	p->CurrentStatus = status;

	for (in = 0; in < IPM_INPUT_MAX; in++)
		IPM_UpdateInput(p, in);
}

//-----------------------------------------------------------------------------
// Evaluate one concrete (device, input, event) triple and fire its callback.
static void IPM_Dispatch(u8 dev, u8 in, u8 event, IPM_cb cb)
{
	IPM_check chk = g_IPM.Checker[event];
	if (chk == NULL)
		return;
	if (chk(dev, in))
	{
		if (cb != NULL)
			cb(dev, in, event);
	}
}

//-----------------------------------------------------------------------------
// Function: IPM_Update
// Sample every supported device, then dispatch all registered event callbacks.
// IPM_DEVICE_ANY / IPM_INPUT_ANY / IPM_EVENT_ANY expand to loops over all
// concrete devices / inputs / event types.
void IPM_Update()
{
	u8 dev, in, ev, e;

	// Refresh state for every enabled device.
	for (dev = 0; dev < IPM_DEVICE_MAX; dev++)
	{
		if (g_IPM.Config.DeviceSupport[dev])
			IPM_UpdateDevice(dev);
	}

	// Dispatch registered events.
	for (e = 0; e < g_IPM.EventsNum; e++)
	{
		IPM_Event* evt = &g_IPM.Events[e];

		u8 dLo = evt->Device, dHi = evt->Device;
		if (evt->Device == IPM_DEVICE_ANY) { dLo = 0; dHi = IPM_DEVICE_MAX - 1; }

		for (dev = dLo; dev <= dHi; dev++)
		{
			if (!g_IPM.Config.DeviceSupport[dev])
				continue;

			u8 iLo = evt->Input, iHi = evt->Input;
			if (evt->Input == IPM_INPUT_ANY) { iLo = 0; iHi = IPM_INPUT_MAX - 1; }

			for (in = iLo; in <= iHi; in++)
			{
				u8 vLo = evt->Event, vHi = evt->Event;
				if (evt->Event == IPM_EVENT_ANY) { vLo = 0; vHi = IPM_EVENT_MAX - 1; }

				for (ev = vLo; ev <= vHi; ev++)
					IPM_Dispatch(dev, in, ev, evt->Callback);
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Function: IPM_RegisterEvent
// Append an event/callback binding to the table if there is room.
bool IPM_RegisterEvent(u8 dev, u8 input, u8 event, IPM_cb cb)
{
	IPM_Event* evt;

	if (g_IPM.EventsNum >= IPM_EVENT_TAB_SIZE - 1)
		return FALSE;

	evt = &g_IPM.Events[g_IPM.EventsNum];
	evt->Device   = dev;
	evt->Input    = input;
	evt->Event    = event;
	evt->Callback = cb;
	g_IPM.EventsNum++;

	return TRUE;
}

//-----------------------------------------------------------------------------
// Function: IPM_GetStickDirection
// Return the current 8-way stick direction (0=up, clockwise to 7=up-left) for a
// device, or 0xFF when no / ambiguous (opposite) directions are pressed.
// Indexed by the pressed-direction bitmask (bit0=U,1=D,2=L,3=R); opposite pairs
// cancel, leaving the net octant.
static const u8 g_IPM_StickDir[16] =
{
	0xFF,	// 0000 none
	0,		// 0001 U
	4,		// 0010 D
	0xFF,	// 0011 U+D (cancel)
	6,		// 0100 L
	7,		// 0101 U+L
	5,		// 0110 D+L
	6,		// 0111 U+D+L -> L
	2,		// 1000 R
	1,		// 1001 U+R
	3,		// 1010 D+R
	2,		// 1011 U+D+R -> R
	0xFF,	// 1100 L+R (cancel)
	0,		// 1101 U+L+R -> U
	4,		// 1110 D+L+R -> D
	0xFF,	// 1111 all
};

u8 IPM_GetStickDirection(u8 dev)
{
	return g_IPM_StickDir[(u8)~g_IPM.Process[dev].CurrentStatus & JOY_INPUT_DIR_MASK];
}

//-----------------------------------------------------------------------------
// Function: IPM_GetEventName
// Return a zero-terminated name for the event id ("" if out of range).
const c8* IPM_GetEventName(u8 ev)
{
	if (ev >= IPM_EVENT_MAX)
		return "???";
	return g_IPM_EventName[ev];
}
