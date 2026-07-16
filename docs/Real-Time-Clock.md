# Real-Time Clock (`clock`)

The MSX2's built-in **RP-5C01** CMOS clock chip. It gives you the date and time — and, more
interestingly for a game, **battery-backed storage that survives a power-off with no disk at all**.
Link `clock.c`, include `clock.h`.

MSX2 and later only (an MSX1 has no RTC).

## The chip

The RTC hangs off two I/O ports — `0xB4` (register select) and `0xB5` (data) — and exposes **four
blocks** of 13 registers, each **4 bits wide**. You pick a block, then read/write registers in it.

| Block | `RTC_MODE_…` | Holds |
|---|---|---|
| 0 | `RTC_MODE_TIME` | seconds, minutes, hours, date |
| 1 | `RTC_MODE_ALARM` | alarm, 12/24-hour flag |
| 2 | `RTC_MODE_SYSTEM` | machine settings (screen colours, area code) |
| 3 | `RTC_MODE_DATA` | **free CMOS RAM — your data** |

Because registers are 4 bits, every decimal digit gets its **own register**: `RTC_REG_TIME_HOUR`
holds the units and `RTC_REG_TIME_10HOUR` the tens. The `RTC_Get…` helpers combine them for you.

## API

```c
void RTC_Initialize(void);          // select the time block
void RTC_Set24H(bool enable);       // 24-hour mode (lives in the ALARM block!)

u8   RTC_GetSecond(void);           // 0-59      u8 RTC_GetDay(void);      // 1-31
u8   RTC_GetMinute(void);           // 0-59      u8 RTC_GetMonth(void);    // 1-12
u8   RTC_GetHour(void);             // 0-23      u8 RTC_GetYear(void);     // 0-99
u8   RTC_GetDayOfWeek(void);        // 0-6       u16 RTC_GetYear4(void);   // 1980-2079

void RTC_SetMode(u8 mode);          // low level: select a block
u8   RTC_Read(u8 reg);              // low level: read one 4-bit register
void RTC_Write(u8 reg, u8 value);   // low level: write one

void RTC_SaveData(u8* data);        // 6 bytes -> battery-backed CMOS
bool RTC_LoadData(u8* data);        // 6 bytes back; FALSE if no valid save
```

## Setting and reading the clock

`set_game_time` seeds the in-game clock; `game_hour`/`game_minute` read it back. Each `RTC_Write`
sets **one BCD digit**. Note the gotcha: `RTC_Set24H` has to switch to the *alarm* block to do its
job, so you must select the time block again afterwards.

```c
// gameclock.c — seed the in-game clock from the MSX real-time clock.
#include "clock.h"

// Set the in-game clock to 13:45 in 24-hour mode.
void set_game_time(void)
{
	RTC_Initialize();          // select the time block (block 0)
	RTC_Set24H(TRUE);

	RTC_Initialize();          // Set24H switched to the alarm block — go back
	RTC_Write(RTC_REG_TIME_10HOUR, 1);   // each register is ONE BCD digit
	RTC_Write(RTC_REG_TIME_HOUR,   3);   // -> 13
	RTC_Write(RTC_REG_TIME_10MIN,  4);
	RTC_Write(RTC_REG_TIME_MIN,    5);   // -> 45
}

u8 game_hour(void)   { return RTC_GetHour(); }
u8 game_minute(void) { return RTC_GetMinute(); }
```

After `set_game_time()`, `game_hour()` reads back `13` and `game_minute()` reads back `45` — each
built from its two BCD digit-registers. *(tested: `clock_01_datetime.c`)*

## Saving 6 bytes without a disk

Block 3 is CMOS RAM kept alive by the machine's battery. `RTC_SaveData` writes **6 bytes** there and
`RTC_LoadData` reads them back, returning `FALSE` if the block holds no valid save. This is how a
cartridge game keeps a high score with no disk drive in the machine at all.

```c
// hiscore_cmos.c — keep a high score in battery-backed CMOS, no disk needed.
#include "clock.h"

// Persist 6 bytes of save data to CMOS block 3.
void hiscore_store(u8* data)
{
	RTC_SaveData(data);                // -> CMOS block 3
}

// Read the 6 bytes back; FALSE if the block holds no valid save.
bool hiscore_fetch(u8* data)
{
	return RTC_LoadData(data);
}
```

Store six bytes with `hiscore_store(...)`, and `hiscore_fetch(...)` reads them back byte-for-byte,
returning `FALSE` only when block 3 has never held a valid save. *(tested: `clock_02_savedata.c`)*

**Six bytes is the whole budget** — enough for a high score and a level number, not a save game.
Check `RTC_LoadData`'s return value: on a machine that has never run your program, the block holds
someone else's data or nothing at all.

## See also

- [MSX-DOS files](MSX-DOS-2.md) — for saves bigger than 6 bytes, when a disk is available.
- [String Conversion](String-Conversion.md) — turning the clock values into text.
