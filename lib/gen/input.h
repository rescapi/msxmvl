// msxmvl — clean-room reimplementation of MSXgl "input" module public API.
// Interface (names, signatures, documented behaviour, port/bit map) taken from
// MSXgl's engine/src/input.h + system_port.h + config_default.h headers ONLY.
// No MSXgl implementation source (.c/.asm/.lst) was read.
//
// User input handler using direct access to I/O ports:
//   - Joystick / general port : PSG registers R#15 (port B, output, port
//     select + pin outputs) and R#14 (port A, input, pins 1..7) via ports
//     0xA0 (register select) / 0xA1 (data write) / 0xA2 (data read).
//   - Keyboard : PPI 8255 port C (0xAA) low nibble selects matrix row, port B
//     (0xA9) reads the 8 column bits (0 = pressed).
//   - Mouse    : general port strobe protocol (pin 8 toggled via PSG R#15,
//     four 4-bit nibbles read from pins 1..4 via R#14).
//
// Toolchain: SDCC --sdcccall 1. The two functions the MSXgl header marks with
// __PRESERVES() / __FASTCALL (Joystick_Read, Keyboard_Read) honour that exact
// contract via naked asm; everything else follows the standard SDCC convention.
#ifndef MSXMVL_INPUT_H
#define MSXMVL_INPUT_H

#include "types.h"

//=============================================================================
// SDCC attribute macros
//=============================================================================
#ifndef __FASTCALL
#define __FASTCALL		__z88dk_fastcall
#endif
#ifndef __NAKED
#define __NAKED			__naked
#endif
#ifndef __PRESERVES
#define __PRESERVES		__preserves_regs
#endif

//=============================================================================
// CONFIG (defaults enable EVERY public function so the whole module compiles).
// All overridable via -D so the same driver can match a real-MSXgl config.
//=============================================================================
#ifndef INPUT_USE_JOYSTICK
#define INPUT_USE_JOYSTICK			TRUE
#endif
#ifndef INPUT_USE_KEYBOARD
#define INPUT_USE_KEYBOARD			TRUE
#endif
#ifndef INPUT_USE_MOUSE
#define INPUT_USE_MOUSE				TRUE
#endif
#ifndef INPUT_USE_DETECT
#define INPUT_USE_DETECT			TRUE
#endif
#ifndef INPUT_USE_ISR_PROTECTION
#define INPUT_USE_ISR_PROTECTION	TRUE
#endif
#ifndef INPUT_JOY_UPDATE
#define INPUT_JOY_UPDATE			TRUE
#endif
#ifndef INPUT_HOLD_SIGNAL
#define INPUT_HOLD_SIGNAL			TRUE
#endif
#ifndef INPUT_KB_UPDATE
#define INPUT_KB_UPDATE				TRUE
#endif
#ifndef INPUT_KB_UPDATE_MIN
#define INPUT_KB_UPDATE_MIN			0
#endif
#ifndef INPUT_KB_UPDATE_MAX
#define INPUT_KB_UPDATE_MAX			10
#endif
#ifndef INPUT_KB_AS_JOYSTICK
#define INPUT_KB_AS_JOYSTICK		TRUE
#endif

//=============================================================================
// I/O ports (from system_port.h)
//=============================================================================
#define P_PSG_REGS				0xA0	// PSG register-select port
__sfr __at(P_PSG_REGS)			g_PSG_RegPort;
#define P_PSG_DATA				0xA1	// PSG data write port
__sfr __at(P_PSG_DATA)			g_PSG_DataPort;
#define P_PSG_STAT				0xA2	// PSG data read port
__sfr __at(P_PSG_STAT)			g_PSG_StatPort;

#define P_PPI_B					0xA9	// keyboard row data (read)
__sfr __at(P_PPI_B)				g_PortReadKeyboard;
#define P_PPI_C					0xAA	// row select / CAP / motor (read/write)
__sfr __at(P_PPI_C)				g_PortAccessKeyboard;

//=============================================================================
// Interrupt protection
//=============================================================================
#if (INPUT_USE_ISR_PROTECTION)
	#define INPUT_DI				di
	#define INPUT_EI				ei
#else
	#define INPUT_DI
	#define INPUT_EI
#endif

#define INPUT_PORT1_LOW				0b00000011
#define INPUT_PORT1_HIGH			0b00010011
#define INPUT_PORT2_LOW				0b01001100
#define INPUT_PORT2_HIGH			0b01101100

enum INPUT_PORT
{
	INPUT_PORT1 = INPUT_PORT1_HIGH,
	INPUT_PORT2 = INPUT_PORT2_HIGH,
};

// Mask
#define INPUT_PIN8_ONLY				0b00110000
#define INPUT_PIN8_MASK				0b11001111
#define INPUT_PORT1_ONLY			0b00010011
#define INPUT_PORT1_MASK			0b11101100
#define INPUT_PORT2_ONLY			0b00101100
#define INPUT_PORT2_MASK			0b11010011

//=============================================================================
// Group: Joystick
//=============================================================================
#if (INPUT_USE_JOYSTICK || INPUT_USE_DETECT)

#define JOY_PORT_1					0b00001111
#define JOY_PORT_2					0b01001111

#define JOY_INPUT_DIR_NONE			0
#define JOY_INPUT_DIR_UP			(1 << 0)
#define JOY_INPUT_DIR_DOWN			(1 << 1)
#define JOY_INPUT_DIR_LEFT			(1 << 2)
#define JOY_INPUT_DIR_RIGHT			(1 << 3)
#define JOY_INPUT_DIR_UP_RIGHT		(JOY_INPUT_DIR_UP + JOY_INPUT_DIR_RIGHT)
#define JOY_INPUT_DIR_UP_LEFT		(JOY_INPUT_DIR_UP + JOY_INPUT_DIR_LEFT)
#define JOY_INPUT_DIR_DOWN_RIGHT	(JOY_INPUT_DIR_DOWN + JOY_INPUT_DIR_RIGHT)
#define JOY_INPUT_DIR_DOWN_LEFT		(JOY_INPUT_DIR_DOWN + JOY_INPUT_DIR_LEFT)
#define JOY_INPUT_DIR_MASK			0x0F
#define JOY_INPUT_DIR_UNCHANGED		0xFF
#define JOY_INPUT_TRIGGER_A			(1 << 4)
#define JOY_INPUT_TRIGGER_B			(1 << 5)
#define JOY_INPUT_TRIGGER_RUN		(JOY_INPUT_DIR_RIGHT + JOY_INPUT_DIR_LEFT)
#define JOY_INPUT_TRIGGER_SELECT	(JOY_INPUT_DIR_UP + JOY_INPUT_DIR_DOWN)

#define IS_JOY_PRESSED(stat, input)	(((stat) & (input)) == 0)
#define IS_JOY_RELEASED(stat, input) (((stat) & (input)) != 0)

#define JOY_GET_DIR(in)				(~(in) & JOY_INPUT_DIR_MASK)
#define JOY_GET_TRIG1(in)			(((in) & JOY_INPUT_TRIGGER_A) == 0)
#define JOY_GET_TRIG2(in)			(((in) & JOY_INPUT_TRIGGER_B) == 0)
#define JOY_GET_A(in)				(((in) & JOY_INPUT_TRIGGER_A) == 0)
#define JOY_GET_B(in)				(((in) & JOY_INPUT_TRIGGER_B) == 0)
#define JOY_GET_RUN(in)				(((in) & JOY_INPUT_TRIGGER_RUN) == 0)
#define JOY_GET_SELECT(in)			(((in) & JOY_INPUT_TRIGGER_SELECT) == 0)

// Gets current joystick information (bit=0: pressed) : xxBARLDU
u8 Joystick_Read(u8 port) __FASTCALL __PRESERVES(b, c, d, e, h, iyl, iyh);

#if (INPUT_JOY_UPDATE)

extern u8 g_JoyStats[2];
extern u8 g_JoyStatsPrev[2];

// Updates both joystick stats at once and stores the result
void Joystick_Update();

inline u8 Joystick_GetDirection(u8 port) { return g_JoyStats[port >> 6] & JOY_INPUT_DIR_MASK; }

inline u8 Joystick_GetDirectionChange(u8 port)
{
	u8 in = g_JoyStats[port >> 6]  & JOY_INPUT_DIR_MASK;
	u8 prev = g_JoyStatsPrev[port >> 6]  & JOY_INPUT_DIR_MASK;
	if (in == prev)
		in = JOY_INPUT_DIR_UNCHANGED;
	return in;
}

inline bool Joystick_IsButtonPressed(u8 port, u8 trigger) { return ((g_JoyStats[port >> 6] & trigger) != 0); }

inline bool Joystick_IsButtonPushed(u8 port, u8 trigger)
{
	u8 in = g_JoyStats[port >> 6];
	u8 prev = g_JoyStatsPrev[port >> 6];
	return ((in & trigger) != 0) && ((prev & trigger) == 0);
}

#else // (!INPUT_JOY_UPDATE)

inline u8 Joystick_GetDirection(u8 port)
{
	u8 in = ~Joystick_Read(port);
	return (in & JOY_INPUT_DIR_MASK);
}

inline bool Joystick_IsButtonPressed(u8 port, u8 trigger)
{
	u8 in = Joystick_Read(port);
	return ((in & trigger) == 0);
}

#endif // (INPUT_JOY_UPDATE)

#endif // (INPUT_USE_JOYSTICK || INPUT_USE_DETECT)

//=============================================================================
// Group: Detect
//=============================================================================
#if (INPUT_USE_DETECT)

enum INPUT_TYPE
{
	INPUT_TYPE_JSX					= 0x0F,
	INPUT_TYPE_JSX_A0_B1			= 0x01,
	INPUT_TYPE_JSX_A2_B1			= 0x09,
	INPUT_TYPE_JSX_A6_B2			= 0x1A,
	INPUT_TYPE_NINJATAP				= 0x1F,
	INPUT_TYPE_MOUSE				= 0x30,
	INPUT_TYPE_JOYMEGA				= 0x33,
	INPUT_TYPE_TRACKBALL			= 0x38,
	INPUT_TYPE_TOUCHPAD				= 0x3D,
	INPUT_TYPE_PADDLE				= 0x3E,
	INPUT_TYPE_JOYSTICK				= 0x3F,
	INPUT_TYPE_LIGHTGUN_ASCII		= 0x20,
	INPUT_TYPE_LIGHTGUN_GUNSTICK	= 0x3F,
	INPUT_TYPE_LIGHTGUN_PHENIX		= 0x37,
	INPUT_TYPE_UNPLUGGED			= INPUT_TYPE_JOYSTICK,
};

// Detect device plugged in General purpose ports
u8 Input_Detect(enum INPUT_PORT port);

#endif

//=============================================================================
// Group: Mouse
//=============================================================================
#if (INPUT_USE_MOUSE)

#define MOUSE_PORT_1				INPUT_PORT1_HIGH
#define MOUSE_PORT_2				INPUT_PORT2_HIGH

#define MOUSE_NOTFOUND				0xFF

#define MOUSE_BOUTON_1				0b00010000
#define MOUSE_BOUTON_2				0b00100000
#define MOUSE_BOUTON_LEFT			MOUSE_BOUTON_1
#define MOUSE_BOUTON_RIGHT			MOUSE_BOUTON_2

enum MOUSE_SPEED
{
	MOUSE_SPEED_LOWEST  = 16,
	MOUSE_SPEED_LOW     = 8,
	MOUSE_SPEED_MEDIUM  = 4,
	MOUSE_SPEED_HIGH    = 2,
	MOUSE_SPEED_HIGHEST = 1,
	MOUSE_SPEED_DEFAULT = MOUSE_SPEED_MEDIUM,
};

typedef struct Mouse_State
{
	u8			Buttons;
	i8			dX;
	i8			dY;
	u8			PrevButtons;
} Mouse_State;

// Gets current mouse information
void Mouse_Read(u8 port, Mouse_State* data);

inline i8 Mouse_GetOffsetX(Mouse_State* data) { return -data->dX; }
inline i8 Mouse_GetOffsetY(Mouse_State* data) { return -data->dY; }
inline i8 Mouse_GetAdjustedOffsetX(Mouse_State* data, u8 spd) { return -data->dX / spd; }
inline i8 Mouse_GetAdjustedOffsetY(Mouse_State* data, u8 spd) { return -data->dY / spd; }
inline bool Mouse_IsButtonPress(Mouse_State* data, u8 btn) { return (data->Buttons & btn) == 0; }
inline bool Mouse_IsButtonClick(Mouse_State* data, u8 btn) { return ((data->Buttons & btn) == 0) && ((data->PrevButtons & btn) != 0); }

#endif // (INPUT_USE_MOUSE)

//=============================================================================
// Group: Keyboard
//=============================================================================
#if (INPUT_USE_KEYBOARD)

#define MAKE_KEY(r, b)		(((b) << 4) | (r))
#define KEY_ROW(key)		((key) & 0x0F)
#define KEY_IDX(key)		((key) >> 4)
#define KEY_FLAG(key)		(1 << KEY_IDX(key))

#define IS_KEY_PRESSED(row, key)  (((row) & KEY_FLAG(key)) == 0)
#define IS_KEY_RELEASED(row, key) (((row) & KEY_FLAG(key)) != 0)
#define IS_KEY_PUSHED(row, prev, key)  ((((row) & KEY_FLAG(key)) == 0) && (((prev) & KEY_FLAG(key)) != 0))

enum KEY_ID
{
	KEY_0		= MAKE_KEY(0, 0),
	KEY_1		= MAKE_KEY(0, 1),
	KEY_2		= MAKE_KEY(0, 2),
	KEY_3		= MAKE_KEY(0, 3),
	KEY_4		= MAKE_KEY(0, 4),
	KEY_5		= MAKE_KEY(0, 5),
	KEY_6		= MAKE_KEY(0, 6),
	KEY_7		= MAKE_KEY(0, 7),
	KEY_8		= MAKE_KEY(1, 0),
	KEY_9		= MAKE_KEY(1, 1),
	KEY_1_2		= MAKE_KEY(1, 2),
	KEY_1_3		= MAKE_KEY(1, 3),
	KEY_1_4		= MAKE_KEY(1, 4),
	KEY_1_5		= MAKE_KEY(1, 5),
	KEY_1_6		= MAKE_KEY(1, 6),
	KEY_1_7		= MAKE_KEY(1, 7),
	KEY_2_0		= MAKE_KEY(2, 0),
	KEY_2_1		= MAKE_KEY(2, 1),
	KEY_2_2		= MAKE_KEY(2, 2),
	KEY_2_3		= MAKE_KEY(2, 3),
	KEY_2_4		= MAKE_KEY(2, 4),
	KEY_2_5		= MAKE_KEY(2, 5),
	KEY_A		= MAKE_KEY(2, 6),
	KEY_B		= MAKE_KEY(2, 7),
	KEY_C		= MAKE_KEY(3, 0),
	KEY_D		= MAKE_KEY(3, 1),
	KEY_E		= MAKE_KEY(3, 2),
	KEY_F		= MAKE_KEY(3, 3),
	KEY_G		= MAKE_KEY(3, 4),
	KEY_H		= MAKE_KEY(3, 5),
	KEY_I		= MAKE_KEY(3, 6),
	KEY_J		= MAKE_KEY(3, 7),
	KEY_K		= MAKE_KEY(4, 0),
	KEY_L		= MAKE_KEY(4, 1),
	KEY_M		= MAKE_KEY(4, 2),
	KEY_N		= MAKE_KEY(4, 3),
	KEY_O		= MAKE_KEY(4, 4),
	KEY_P		= MAKE_KEY(4, 5),
	KEY_Q		= MAKE_KEY(4, 6),
	KEY_R		= MAKE_KEY(4, 7),
	KEY_S		= MAKE_KEY(5, 0),
	KEY_T		= MAKE_KEY(5, 1),
	KEY_U		= MAKE_KEY(5, 2),
	KEY_V		= MAKE_KEY(5, 3),
	KEY_W		= MAKE_KEY(5, 4),
	KEY_X		= MAKE_KEY(5, 5),
	KEY_Y		= MAKE_KEY(5, 6),
	KEY_Z		= MAKE_KEY(5, 7),
	KEY_SHIFT	= MAKE_KEY(6, 0),
	KEY_CTRL	= MAKE_KEY(6, 1),
	KEY_GRAPH	= MAKE_KEY(6, 2),
	KEY_CAPS	= MAKE_KEY(6, 3),
	KEY_CODE	= MAKE_KEY(6, 4),
	KEY_F1		= MAKE_KEY(6, 5),
	KEY_F2		= MAKE_KEY(6, 6),
	KEY_F3		= MAKE_KEY(6, 7),
	KEY_F4		= MAKE_KEY(7, 0),
	KEY_F5		= MAKE_KEY(7, 1),
	KEY_ESC		= MAKE_KEY(7, 2),
	KEY_TAB		= MAKE_KEY(7, 3),
	KEY_STOP	= MAKE_KEY(7, 4),
	KEY_BS		= MAKE_KEY(7, 5),
	KEY_SELECT	= MAKE_KEY(7, 6),
	KEY_RETURN	= MAKE_KEY(7, 7),
	KEY_SPACE	= MAKE_KEY(8, 0),
	KEY_HOME	= MAKE_KEY(8, 1),
	KEY_INS		= MAKE_KEY(8, 2),
	KEY_DEL		= MAKE_KEY(8, 3),
	KEY_LEFT	= MAKE_KEY(8, 4),
	KEY_UP		= MAKE_KEY(8, 5),
	KEY_DOWN	= MAKE_KEY(8, 6),
	KEY_RIGHT	= MAKE_KEY(8, 7),
	KEY_NUM_MUL	= MAKE_KEY(9, 0),
	KEY_NUM_ADD	= MAKE_KEY(9, 1),
	KEY_NUM_DIV	= MAKE_KEY(9, 2),
	KEY_NUM_0	= MAKE_KEY(9, 3),
	KEY_NUM_1	= MAKE_KEY(9, 4),
	KEY_NUM_2	= MAKE_KEY(9, 5),
	KEY_NUM_3	= MAKE_KEY(9, 6),
	KEY_NUM_4	= MAKE_KEY(9, 7),
	KEY_NUM_5	= MAKE_KEY(10, 0),
	KEY_NUM_6	= MAKE_KEY(10, 1),
	KEY_NUM_7	= MAKE_KEY(10, 2),
	KEY_NUM_8	= MAKE_KEY(10, 3),
	KEY_NUM_9	= MAKE_KEY(10, 4),
	KEY_NUM_MIN	= MAKE_KEY(10, 5),
	KEY_NUM_COM	= MAKE_KEY(10, 6),
	KEY_NUM_DOT	= MAKE_KEY(10, 7),
};
#define KEY_RET			KEY_RETURN
#define KEY_ENTER		KEY_RETURN
#define KEY_BACK		KEY_BS

// Reads keyboard matrix row (bit=0: pressed)
u8 Keyboard_Read(u8 row) __FASTCALL __PRESERVES(b, c, d, e, h, iyl, iyh);

#if (INPUT_KB_UPDATE)

extern u8* g_InputBufferNew;
extern u8* g_InputBufferOld;

inline void Keyboard_SetBuffer(u8* new, u8* old) { g_InputBufferNew = new; g_InputBufferOld = old; }

// Updates all keyboard rows at once
void Keyboard_Update();

inline bool Keyboard_IsKeyPressed(u8 key) { return (g_InputBufferNew[KEY_ROW(key)] & (1 << KEY_IDX(key))) == 0; }

// Checks if a given key was just pushed
bool Keyboard_IsKeyPushed(u8 key);

#else

inline bool Keyboard_IsKeyPressed(u8 key) { return (Keyboard_Read(KEY_ROW(key)) & (1 << KEY_IDX(key))) == 0; }

#endif // (INPUT_KB_UPDATE)

#if (INPUT_KB_AS_JOYSTICK)

// Emulate joystick input using keyboard keys (bit=0: pressed) : xxBARLDU
u8 Keyboard_ReadAsJoystick();

#endif // (INPUT_KB_AS_JOYSTICK)

#endif // (INPUT_USE_KEYBOARD)

#endif // MSXMVL_INPUT_H
