// msxmvl — clean-room reimplementation of MSXgl "clock" module public API.
// Interface (names, signatures, documented behaviour, register/port map) taken
// from MSXgl's engine/src/clock.h + system_port.h headers ONLY. No MSXgl
// implementation source (.c/.asm/.lst) was read.
//
// Real-time clock module for the Ricoh RP-5C01 (MSX built-in CMOS RTC).
// Accessed through two I/O ports:
//   0xB4 (address/register-select) and 0xB5 (data, 4 bits usable).
// The chip exposes 4 "blocks" (modes) selected via the mode register (0xD);
// each block holds 13 four-bit registers (0x0..0xC).
//
// Toolchain: SDCC --sdcccall 1. The MSXgl clock header declares no
// __PRESERVES() contract, so the bodies below follow the standard SDCC calling
// convention (AF/BC/DE/HL/IX caller-clobberable, IY preserved by SDCC).
#ifndef MSXMVL_CLOCK_H
#define MSXMVL_CLOCK_H

#include "types.h"

//=============================================================================
// CONFIG (defaults mirror MSXgl clock.h)
//=============================================================================
#ifndef RTC_USE_CLOCK
#define RTC_USE_CLOCK			TRUE
#endif
#ifndef RTC_USE_CLOCK_EXTRA
#define RTC_USE_CLOCK_EXTRA		TRUE
#endif
#ifndef RTC_USE_SAVEDATA
#define RTC_USE_SAVEDATA		TRUE
#endif
#ifndef RTC_USE_SAVESIGNED
#define RTC_USE_SAVESIGNED		TRUE
#endif

//=============================================================================
// RTC I/O PORTS (from system_port.h)
//=============================================================================
#define P_RTC_ADDR				0xB4   // RTC address / register-select port
__sfr __at(P_RTC_ADDR)			g_RTC_AddrPort;
#define P_RTC_DATA				0xB5   // RTC data port (4 bits usable)
__sfr __at(P_RTC_DATA)			g_RTC_DataPort;

//=============================================================================
// MODE REGISTER (0xD)
//=============================================================================
#define RTC_REG_MODE			0xD

#define RTC_MODE_BLOCK_0		0
#define RTC_MODE_BLOCK_1		1
#define RTC_MODE_BLOCK_2		2
#define RTC_MODE_BLOCK_3		3
#define RTC_MODE_ALARM_ON		4
#define RTC_MODE_ALARM_OFF		0
#define RTC_MODE_TIMER_ON		8
#define RTC_MODE_TIMER_OFF		0

//=============================================================================
// TEST REGISTER (0xE)
//=============================================================================
#define RTC_REG_TEST			0xE
#define RTC_TEST_SEC			(1<<0)
#define RTC_TEST_MIN			(1<<1)
#define RTC_TEST_HOUR			(1<<2)
#define RTC_TEST_DAY			(1<<3)

//=============================================================================
// RESET REGISTER (0xF)
//=============================================================================
#define RTC_REG_RESET			0xF
#define RTC_RESET_ALARM			(1<<0)
#define RTC_RESET_SEC			(1<<1)
#define RTC_RESET_16HZ			(1<<2)
#define RTC_RESET_1HZ			(1<<3)

//=============================================================================
// BLOCK 0 - TIME
//=============================================================================
#define RTC_MODE_TIME			RTC_MODE_BLOCK_0

#define RTC_REG_TIME_SEC		0x0
#define RTC_REG_TIME_10SEC		0x1
#define RTC_REG_TIME_MIN		0x2
#define RTC_REG_TIME_10MIN		0x3
#define RTC_REG_TIME_HOUR		0x4
#define RTC_REG_TIME_10HOUR		0x5
#define RTC_REG_TIME_WEEKDAY	0x6
#define RTC_REG_TIME_DAY		0x7
#define RTC_REG_TIME_10DAY		0x8
#define RTC_REG_TIME_MONTH		0x9
#define RTC_REG_TIME_10MONTH	0xA
#define RTC_REG_TIME_YEAR		0xB
#define RTC_REG_TIME_10YEAR		0xC

#define RTC_DAY_SUNDAY			0
#define RTC_DAY_MONDAY			1
#define RTC_DAY_TUESDAY			2
#define RTC_DAY_WEDNESDAY		3
#define RTC_DAY_THURSDAY		4
#define RTC_DAY_FRIDAY			5
#define RTC_DAY_SATURDAY		6

//=============================================================================
// BLOCK 1 - ALARM
//=============================================================================
#define RTC_MODE_ALARM			RTC_MODE_BLOCK_1

#define RTC_REG_ALARM_MIN		0x2
#define RTC_REG_ALARM_10MIN		0x3
#define RTC_REG_ALARM_HOUR		0x4
#define RTC_REG_ALARM_10HOUR	0x5
#define RTC_REG_ALARM_WEEKDAY	0x6
#define RTC_REG_ALARM_DAY		0x7
#define RTC_REG_ALARM_10DAY		0x8
#define RTC_REG_ALARM_24H		0xA
#define RTC_REG_ALARM_YEAR		0xB

#define RTC_HOURMODE_12H		0
#define RTC_HOURMODE_24H		1

//=============================================================================
// BLOCK 2 - SYSTEM SETTING
//=============================================================================
#define RTC_MODE_SYSTEM			RTC_MODE_BLOCK_2
#define RTC_MODE_SCREEN			RTC_MODE_BLOCK_2

#define RTC_REG_SYS_INIT		0x0
#define RTC_REG_SYS_X_ADJUST	0x1
#define RTC_REG_SYS_Y_ADJUST	0x2
#define RTC_REG_SYS_SCREEN		0x3
#define RTC_REG_SYS_WIDTH_LSB	0x4
#define RTC_REG_SYS_WIDTH_MSB	0x5
#define RTC_REG_SYS_TXT			0x6
#define RTC_REG_SYS_BG			0x7
#define RTC_REG_SYS_BORDER		0x8
#define RTC_REG_SYS_KEY			0x9
#define RTC_REG_SYS_BEEP		0xA
#define RTC_REG_SYS_LOGO		0xB
#define RTC_REG_SYS_AREA_CODE	0xC

#define RTC_INIT_DONE			0b1010

#define RTC_AREA_JAPAN			0
#define RTC_AREA_US				1
#define RTC_AREA_INT			2
#define RTC_AREA_GB				3
#define RTC_AREA_FRANCE			4
#define RTC_AREA_GERMANY		5
#define RTC_AREA_ITALY			6
#define RTC_AREA_SPAIN			7
#define RTC_AREA_AE				8
#define RTC_AREA_KOREA			9
#define RTC_AREA_USSR			10

//=============================================================================
// BLOCK 3 - TEXT / DATA
//=============================================================================
#define RTC_MODE_TEXT			RTC_MODE_BLOCK_3
#define RTC_MODE_DATA			RTC_MODE_BLOCK_3

#define RTC_REG_DATATYPE		0x0

#define RTC_DATA_TITLE			0
#define RTC_DATA_PASSWORD		1
#define RTC_DATA_PROMPT			2
#define RTC_DATA_SAVE			6
#define RTC_DATA_SIGNSAVE		7
#define RTC_DATA_INVALID		15

//=============================================================================
// Group: Core (inline, defined exactly like MSXgl clock.h)
//=============================================================================

// Set clock mode (block select). Timer is always enabled, alarm disabled.
inline void RTC_SetMode(u8 mode)
{
	g_RTC_AddrPort = RTC_REG_MODE;
	g_RTC_DataPort = mode | (RTC_MODE_ALARM_OFF + RTC_MODE_TIMER_ON);
}

// Read a RTC register value (4 bits).
inline u8 RTC_Read(u8 reg)
{
	g_RTC_AddrPort = reg;
	return 0xF & g_RTC_DataPort;
}

// Write a RTC register value.
inline void RTC_Write(u8 reg, u8 value)
{
	g_RTC_AddrPort = reg;
	g_RTC_DataPort = value;
}

// Write data in CMOS in the current block (1..6 bytes as raw nibbles).
void RTC_WriteRaw(u8 reg, u8* data, u8 num);

// Read data from CMOS in the current block (1..6 bytes as raw nibbles).
void RTC_ReadRaw(u8 reg, u8* data, u8 num);

//=============================================================================
// Group: Date/Time
//=============================================================================

// Initialize the clock module (select the time block).
inline void RTC_Initialize(void) { RTC_SetMode(RTC_MODE_TIME); }

#if (RTC_USE_CLOCK)

// Set 12/24 hours mode (TRUE for 24h).
inline void RTC_Set24H(bool enable)
{
	RTC_SetMode(RTC_MODE_ALARM);
	RTC_Write(RTC_REG_ALARM_24H, enable ? 1 : 0);
}

u8 RTC_GetSecond(void);		// 0-59
u8 RTC_GetMinute(void);		// 0-59
u8 RTC_GetHour(void);		// 0-23

// Check if current hour is PM (>= 12) or AM.
inline bool RTC_IsPM(void) { return RTC_GetHour() >= 12; }

u8 RTC_GetDayOfWeek(void);	// 0-6
u8 RTC_GetDay(void);		// 1-31
u8 RTC_GetMonth(void);		// 1-12
u8 RTC_GetYear(void);		// 0-99

//-----------------------------------------------------------------------------
// Group: Extra
//-----------------------------------------------------------------------------
#if (RTC_USE_CLOCK_EXTRA)

const c8* RTC_GetDayOfWeekString(void);
const c8* RTC_GetMonthString(void);
u16 RTC_GetYear4(void);		// 1980-2079

#endif // (RTC_USE_CLOCK_EXTRA)
#endif // (RTC_USE_CLOCK)

//=============================================================================
// Group: System Setting
//=============================================================================

// Check if setting data (block 2) is valid.
inline bool RTC_IsSettingOK(void)
{
	RTC_SetMode(RTC_MODE_SCREEN);
	return RTC_Read(RTC_REG_SYS_INIT) == RTC_INIT_DONE;
}

// Set area code.
inline void RTC_SetAreaCode(u8 code)
{
	RTC_SetMode(RTC_MODE_SCREEN);
	RTC_Write(RTC_REG_SYS_AREA_CODE, code);
}

// Get area code.
inline u8 RTC_GetAreaCode(void)
{
	RTC_SetMode(RTC_MODE_SCREEN);
	return RTC_Read(RTC_REG_SYS_AREA_CODE);
}

//=============================================================================
// Group: Custom Data
//=============================================================================

// Get current data type (see RTC_DATA_ defines).
inline u8 RTC_GetDataType(void)
{
	RTC_SetMode(RTC_MODE_DATA);
	return RTC_Read(RTC_REG_DATATYPE);
}

#if (RTC_USE_SAVEDATA)

// Save 6 bytes data to CMOS's block 3.
void RTC_SaveData(u8* data);

// Load 6 bytes data from CMOS's block 3. FALSE if load failed.
bool RTC_LoadData(u8* data);

#endif // (RTC_USE_SAVEDATA)

#if (defined(APPSIGN) && RTC_USE_SAVESIGNED)

// Save 6 bytes data to CMOS's block 3 with the application signature.
void RTC_SaveDataSigned(u8* data);

// Load 6 bytes data from CMOS's block 3, verifying the application signature.
bool RTC_LoadDataSigned(u8* data);

#endif // (defined(APPSIGN) && RTC_USE_SAVESIGNED)

#endif // MSXMVL_CLOCK_H
