// msxmvl — clean-room reimplementation of MSXgl "clock" module (bodies).
// Signatures + documented behaviour from MSXgl engine/src/clock.h only.
// No MSXgl implementation source was read.
//
// The RP-5C01 stores each decimal field as two BCD nibbles in separate 4-bit
// registers: a "units" register and a "tens" register. A field value is
// therefore reconstructed as (tens * 10 + units). Each register read already
// masks to 4 bits (see RTC_Read).
#include "clock.h"

//=============================================================================
// Group: Core - raw block access
//=============================================================================

// Copy 'num' bytes (1..6) into the current block. Each full byte is stored as
// two 4-bit registers: the low nibble in register 'reg', the high nibble in the
// next register. So 'num' bytes occupy 2*num consecutive registers.
void RTC_WriteRaw(u8 reg, u8* data, u8 num)
{
	for (u8 i = 0; i < num; ++i)
	{
		RTC_Write(reg++, *data & 0xF);
		RTC_Write(reg++, *data >> 4);
		data++;
	}
}

// Read 'num' bytes (1..6) from the current block. Each byte is reconstructed
// from two consecutive 4-bit registers (low nibble from 'reg', high nibble from
// the next register), so 'num' bytes span 2*num registers.
void RTC_ReadRaw(u8 reg, u8* data, u8 num)
{
	for (u8 i = 0; i < num; ++i)
	{
		*data = RTC_Read(reg++);
		*data |= RTC_Read(reg++) << 4;
		data++;
	}
}

#if (RTC_USE_CLOCK)

//=============================================================================
// Group: Date/Time getters (block 0)
//=============================================================================

u8 RTC_GetSecond(void)
{
	RTC_SetMode(RTC_MODE_TIME);
	return RTC_Read(RTC_REG_TIME_10SEC) * 10 + RTC_Read(RTC_REG_TIME_SEC);
}

u8 RTC_GetMinute(void)
{
	RTC_SetMode(RTC_MODE_TIME);
	return RTC_Read(RTC_REG_TIME_10MIN) * 10 + RTC_Read(RTC_REG_TIME_MIN);
}

u8 RTC_GetHour(void)
{
	RTC_SetMode(RTC_MODE_TIME);
	return RTC_Read(RTC_REG_TIME_10HOUR) * 10 + RTC_Read(RTC_REG_TIME_HOUR);
}

u8 RTC_GetDayOfWeek(void)
{
	RTC_SetMode(RTC_MODE_TIME);
	return RTC_Read(RTC_REG_TIME_WEEKDAY);
}

u8 RTC_GetDay(void)
{
	RTC_SetMode(RTC_MODE_TIME);
	return RTC_Read(RTC_REG_TIME_10DAY) * 10 + RTC_Read(RTC_REG_TIME_DAY);
}

u8 RTC_GetMonth(void)
{
	RTC_SetMode(RTC_MODE_TIME);
	return RTC_Read(RTC_REG_TIME_10MONTH) * 10 + RTC_Read(RTC_REG_TIME_MONTH);
}

u8 RTC_GetYear(void)
{
	RTC_SetMode(RTC_MODE_TIME);
	return RTC_Read(RTC_REG_TIME_10YEAR) * 10 + RTC_Read(RTC_REG_TIME_YEAR);
}

//=============================================================================
// Group: Extra
//=============================================================================
#if (RTC_USE_CLOCK_EXTRA)

static const c8* const g_RTC_DayName[7] =
{
	"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
};

static const c8* const g_RTC_MonthName[12] =
{
	"January", "February", "March", "April", "May", "June",
	"July", "August", "September", "October", "November", "December"
};

const c8* RTC_GetDayOfWeekString(void)
{
	return g_RTC_DayName[RTC_GetDayOfWeek()];
}

const c8* RTC_GetMonthString(void)
{
	return g_RTC_MonthName[RTC_GetMonth() - 1];
}

// The MSX RTC year counter (0-99) is an offset from base year 1980, giving a
// four-digit range of 1980-2079.
u16 RTC_GetYear4(void)
{
	return 1980 + RTC_GetYear();
}

#endif // (RTC_USE_CLOCK_EXTRA)
#endif // (RTC_USE_CLOCK)

//=============================================================================
// Group: Custom Data (block 3)
//=============================================================================
// A byte buffer of 6 bytes is stored as 12 nibbles across registers 0x1..0xC,
// low nibble first, with register 0x0 holding the data-type marker.

#if (RTC_USE_SAVEDATA)

void RTC_SaveData(u8* data)
{
	RTC_SetMode(RTC_MODE_BLOCK_3);
	RTC_Write(0, RTC_DATA_SAVE);
	RTC_WriteRaw(1, data, 6);
}

bool RTC_LoadData(u8* data)
{
	RTC_SetMode(RTC_MODE_BLOCK_3);
	if (RTC_Read(0) != RTC_DATA_SAVE) // Check data type
		return FALSE;
	RTC_ReadRaw(1, data, 6);
	return TRUE;
}

#endif // (RTC_USE_SAVEDATA)

#if (defined(APPSIGN) && RTC_USE_SAVESIGNED)

// Signed variants tag the block with RTC_DATA_SIGNSAVE and XOR the stored
// nibbles with the low byte of the application signature so that another
// application (with a different APPSIGN) reads back scrambled data. This keeps
// the full 6-byte payload while tying it to the current application.
void RTC_SaveDataSigned(u8* data)
{
	u8 i;
	u8 reg = 1;
	u8 sign = (u8)(APPSIGN);
	RTC_SetMode(RTC_MODE_DATA);
	RTC_Write(RTC_REG_DATATYPE, RTC_DATA_SIGNSAVE);
	for (i = 0; i < 6; i++)
	{
		u8 v = (u8)(data[i] ^ sign);
		RTC_Write(reg++, v & 0x0F);
		RTC_Write(reg++, v >> 4);
	}
}

bool RTC_LoadDataSigned(u8* data)
{
	u8 i;
	u8 reg = 1;
	u8 sign = (u8)(APPSIGN);
	RTC_SetMode(RTC_MODE_DATA);
	if (RTC_Read(RTC_REG_DATATYPE) != RTC_DATA_SIGNSAVE)
		return FALSE;
	for (i = 0; i < 6; i++)
	{
		u8 lo = RTC_Read(reg++);
		u8 hi = RTC_Read(reg++);
		data[i] = (u8)((lo | (hi << 4)) ^ sign);
	}
	return TRUE;
}

#endif // (defined(APPSIGN) && RTC_USE_SAVESIGNED)
