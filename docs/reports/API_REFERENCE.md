# msxmvl Complete API Reference

All 796 functions across 35 modules. Each shows its signature and verification status
(`verified` = byte-differential-matched vs MSXgl; `verified_divergent` = proven correct where a byte
match is structurally impossible — see compat/FAILURES.md). Clean-room reimplementation of MSXgl.

## basic_usr (9 functions)

| function | signature | status |
|---|---|---|
| `Basic_GetByte` | `i8 Basic_GetByte(void)` | verified |
| `Basic_GetString` | `const c8* Basic_GetString(void)` | verified |
| `Basic_GetStringLength` | `u8 Basic_GetStringLength(void)` | verified |
| `Basic_GetType` | `u8 Basic_GetType(void)` | verified |
| `Basic_GetWord` | `i16 Basic_GetWord(void)` | verified |
| `Basic_SetByte` | `void Basic_SetByte(i8 val)` | verified |
| `Basic_SetFloat` | `void Basic_SetFloat(f32 val)` | verified |
| `Basic_SetString` | `void Basic_SetString(const c8* val, u8 len)` | verified |
| `Basic_SetWord` | `void Basic_SetWord(i16 val)` | verified |

## bios (89 functions)

| function | signature | status |
|---|---|---|
| `BIOS_ApplyBorder` | `inline void BIOS_ApplyBorder(u8 border)` | verified |
| `BIOS_ApplyColor` | `inline void BIOS_ApplyColor(u8 text, u8 back, u8 border)` | verified |
| `BIOS_Beep` | `inline void BIOS_Beep()` | verified |
| `BIOS_ClearScreen` | `void BIOS_ClearScreen()` | verified |
| `BIOS_ClearSprites` | `inline void BIOS_ClearSprites()` | verified |
| `BIOS_CopyFromVRAM` | `void BIOS_CopyFromVRAM(u16 vram, void* ram, u16 length)` | verified |
| `BIOS_CopyToVRAM` | `void BIOS_CopyToVRAM(const void* ram, u16 vram, u16 length)` | verified |
| `BIOS_DisableScreen` | `inline void BIOS_DisableScreen()` | verified |
| `BIOS_DisplayScreen` | `inline void BIOS_DisplayScreen(bool enable)` | verified |
| `BIOS_EnableScreen` | `inline void BIOS_EnableScreen()` | verified |
| `BIOS_Exit` | `void BIOS_Exit(u8 ret)` | verified_divergent |
| `BIOS_FillVRAM` | `void BIOS_FillVRAM(u16 addr, u16 length, u8 value)` | verified |
| `BIOS_GetCPUMode` | `inline u8 BIOS_GetCPUMode()` | verified_divergent |
| `BIOS_GetCharAddress` | `inline u16 BIOS_GetCharAddress(u8 chr)` | verified |
| `BIOS_GetCharacter` | `inline c8 BIOS_GetCharacter()` | verified |
| `BIOS_GetFontAddress` | `inline u16 BIOS_GetFontAddress()` | verified |
| `BIOS_GetJoystickDirection` | `inline u8 BIOS_GetJoystickDirection(u8 port)` | verified |
| `BIOS_GetJoystickTrigger` | `inline bool BIOS_GetJoystickTrigger(u8 trigger)` | verified |
| `BIOS_GetKeyboardMatrix` | `inline u8 BIOS_GetKeyboardMatrix(u8 line)` | verified |
| `BIOS_GetMSXVersion` | `inline u8 BIOS_GetMSXVersion()` | verified |
| `BIOS_GetPaddle` | `inline u8 BIOS_GetPaddle(u8 num)` | verified |
| `BIOS_GetSpriteAttributeAddress` | `u16 BIOS_GetSpriteAttributeAddress(u8 id)` | verified |
| `BIOS_GetSpriteOverScanId` | `inline u8 BIOS_GetSpriteOverScanId()` | verified |
| `BIOS_GetSpritePatternAddress` | `u16 BIOS_GetSpritePatternAddress(u8 id)` | verified |
| `BIOS_GetSpriteSize` | `inline u8 BIOS_GetSpriteSize()` | verified |
| `BIOS_GetTouchPad` | `inline u8 BIOS_GetTouchPad(u8 entry)` | verified |
| `BIOS_GetVDPReadPort` | `inline u8 BIOS_GetVDPReadPort()` | verified |
| `BIOS_GetVDPWritePort` | `inline u8 BIOS_GetVDPWritePort()` | verified |
| `BIOS_GraphPrint` | `inline void BIOS_GraphPrint(const c8* str)` | verified |
| `BIOS_GraphPrintAt` | `inline void BIOS_GraphPrintAt(u16 x, u16 y, const c8* str)` | verified |
| `BIOS_GraphPrintChar` | `inline void BIOS_GraphPrintChar(u8 chr)` | verified |
| `BIOS_GraphSetCursor` | `inline void BIOS_GraphSetCursor(u16 x, u16 y)` | verified |
| `BIOS_GraphSetOperator` | `inline void BIOS_GraphSetOperator(u8 op)` | verified |
| `BIOS_HasCharacter` | `u8 BIOS_HasCharacter()` | verified |
| `BIOS_HideSprite` | `inline void BIOS_HideSprite(u8 id)` | verified |
| `BIOS_InitPSG` | `inline void BIOS_InitPSG()` | verified |
| `BIOS_InitScreen0` | `inline void BIOS_InitScreen0()` | verified |
| `BIOS_InitScreen0Color` | `inline void BIOS_InitScreen0Color(u8 width, u8 text, u8 back, u8 border)` | verified |
| `BIOS_InitScreen0Ex` | `inline void BIOS_InitScreen0Ex(u16 pnt, u16 pgt, u8 width, u8 text, u8 back, u8 border)` | verified |
| `BIOS_InitScreen1` | `inline void BIOS_InitScreen1()` | verified |
| `BIOS_InitScreen1Color` | `inline void BIOS_InitScreen1Color(u8 width, u8 text, u8 back, u8 border)` | verified |
| `BIOS_InitScreen1Ex` | `inline void BIOS_InitScreen1Ex(u16 pnt, u16 ct, u16 pgt, u16 sat, u16 sgt, u8 width, u8 text, u8 back, u8 border)` | verified |
| `BIOS_InitScreen2` | `inline void BIOS_InitScreen2()` | verified |
| `BIOS_InitScreen2Color` | `inline void BIOS_InitScreen2Color(u8 width, u8 text, u8 back, u8 border)` | verified |
| `BIOS_InitScreen2Ex` | `inline void BIOS_InitScreen2Ex(u16 pnt, u16 ct, u16 pgt, u16 sat, u16 sgt, u8 width, u8 text, u8 back, u8 border)` | verified |
| `BIOS_InitScreen3` | `inline void BIOS_InitScreen3()` | verified |
| `BIOS_InitScreen3Ex` | `void BIOS_InitScreen3Ex(u16 pnt, u16 ct, u16 pgt, u16 sat, u16 sgt, u8 text, u8 bg, u8 border)` | verified |
| `BIOS_InterSlotCall` | `void BIOS_InterSlotCall(u8 slot, u16 addr)` | verified_divergent |
| `BIOS_InterSlotRead` | `u8 BIOS_InterSlotRead(u8 slot, u16 addr)` | verified |
| `BIOS_InterSlotWrite` | `void BIOS_InterSlotWrite(u8 slot, u16 addr, u8 value)` | verified_divergent |
| `BIOS_IsKeyPressed` | `inline bool BIOS_IsKeyPressed(u8 key)` | verified |
| `BIOS_IsPrinterReady` | `inline bool BIOS_IsPrinterReady()` | verified |
| `BIOS_IsSpriteCollision` | `inline bool BIOS_IsSpriteCollision()` | verified |
| `BIOS_IsSpriteOverScan` | `inline bool BIOS_IsSpriteOverScan()` | verified |
| `BIOS_PlayPSG` | `inline void BIOS_PlayPSG()` | verified |
| `BIOS_PrinterChangePage` | `inline void BIOS_PrinterChangePage()` | verified_divergent |
| `BIOS_PrinterOutput` | `inline void BIOS_PrinterOutput(u8 chr)` | verified_divergent |
| `BIOS_PrinterSendChar` | `inline void BIOS_PrinterSendChar(u8 chr)` | verified_divergent |
| `BIOS_PrinterSendString` | `inline void BIOS_PrinterSendString(const u8* str)` | verified_divergent |
| `BIOS_ReadPSG` | `inline u8 BIOS_ReadPSG(u8 reg)` | verified |
| `BIOS_ReadVDP` | `inline u8 BIOS_ReadVDP(u8 reg)` | verified |
| `BIOS_ReadVRAM` | `inline u8 BIOS_ReadVRAM(u16 addr)` | verified_divergent |
| `BIOS_Reboot` | `inline void BIOS_Reboot()` | verified_divergent |
| `BIOS_SetAddressForRead` | `inline void BIOS_SetAddressForRead(u16 addr)` | verified |
| `BIOS_SetAddressForWrite` | `inline void BIOS_SetAddressForWrite(u16 addr)` | verified |
| `BIOS_SetCPUMode` | `inline void BIOS_SetCPUMode(u8 mode)` | verified_divergent |
| `BIOS_SetColor` | `inline void BIOS_SetColor(u8 text, u8 back, u8 border)` | verified |
| `BIOS_SetKeyClick` | `inline void BIOS_SetKeyClick(bool enable)` | verified |
| `BIOS_SetScreen0` | `inline void BIOS_SetScreen0()` | verified |
| `BIOS_SetScreen1` | `inline void BIOS_SetScreen1()` | verified |
| `BIOS_SetScreen2` | `inline void BIOS_SetScreen2()` | verified |
| `BIOS_SetScreen3` | `inline void BIOS_SetScreen3()` | verified |
| `BIOS_SetScreenMode` | `inline void BIOS_SetScreenMode(u8 screen)` | verified |
| `BIOS_SetSprite` | `inline void BIOS_SetSprite(u8 id, u8 x, u8 y, u8 pat, u8 color)` | verified |
| `BIOS_SetSpriteColor` | `inline void BIOS_SetSpriteColor(u8 id, u8 color)` | verified |
| `BIOS_SetSpriteData` | `inline void BIOS_SetSpriteData(u8 id, const BIOS_SpriteAttributes* attrs)` | verified |
| `BIOS_SetSpriteMode` | `inline void BIOS_SetSpriteMode(u8 size, u8 mag)` | verified |
| `BIOS_SetSpritePattern` | `inline void BIOS_SetSpritePattern(u8 id, u8 pat)` | verified |
| `BIOS_SetSpritePosition` | `inline void BIOS_SetSpritePosition(u8 id, u8 x, u8 y)` | verified |
| `BIOS_SetWidth32` | `inline void BIOS_SetWidth32(u8 width)` | verified |
| `BIOS_SetWidth40` | `inline void BIOS_SetWidth40(u8 width)` | verified |
| `BIOS_SwitchSlot` | `void BIOS_SwitchSlot(u8 page, u8 slot)` | verified |
| `BIOS_TextPrint` | `inline void BIOS_TextPrint(const c8* str)` | verified |
| `BIOS_TextPrintAt` | `inline void BIOS_TextPrintAt(u8 x, u8 y, const c8* str)` | verified |
| `BIOS_TextPrintChar` | `inline void BIOS_TextPrintChar(c8 chr)` | verified |
| `BIOS_TextSetCursor` | `void BIOS_TextSetCursor(u8 x, u8 y)` | verified |
| `BIOS_WritePSG` | `void BIOS_WritePSG(u8 reg, u8 value)` | verified |
| `BIOS_WriteVDP` | `void BIOS_WriteVDP(u8 reg, u8 value)` | verified |
| `BIOS_WriteVRAM` | `void BIOS_WriteVRAM(u8 value, u16 addr)` | verified |

## bios_hook (5 functions)

| function | signature | status |
|---|---|---|
| `BIOS_BackupHook` | `inline void BIOS_BackupHook(u16 hook, void* buffer)` | verified |
| `BIOS_ClearHook` | `inline void BIOS_ClearHook(u16 hook)` | verified |
| `BIOS_SetHookCallback` | `void BIOS_SetHookCallback(u16 hook, callback cb)` | verified |
| `BIOS_SetHookDirectCallback` | `inline void BIOS_SetHookDirectCallback(u16 hook, callback cb)` | verified |
| `BIOS_SetHookInterSlotCallback` | `inline void BIOS_SetHookInterSlotCallback(u16 hook, u32 slot, callback cb)` | verified |

## clock (26 functions)

| function | signature | status |
|---|---|---|
| `RTC_GetAreaCode` | `inline u8 RTC_GetAreaCode(void)` | verified |
| `RTC_GetDataType` | `inline u8 RTC_GetDataType(void)` | verified |
| `RTC_GetDay` | `u8 RTC_GetDay(void)` | verified |
| `RTC_GetDayOfWeek` | `u8 RTC_GetDayOfWeek(void)` | verified |
| `RTC_GetDayOfWeekString` | `const c8* RTC_GetDayOfWeekString(void)` | verified |
| `RTC_GetHour` | `u8 RTC_GetHour(void)` | verified |
| `RTC_GetMinute` | `u8 RTC_GetMinute(void)` | verified |
| `RTC_GetMonth` | `u8 RTC_GetMonth(void)` | verified |
| `RTC_GetMonthString` | `const c8* RTC_GetMonthString(void)` | verified |
| `RTC_GetSecond` | `u8 RTC_GetSecond(void)` | verified |
| `RTC_GetYear` | `u8 RTC_GetYear(void)` | verified |
| `RTC_GetYear4` | `u16 RTC_GetYear4(void)` | verified |
| `RTC_Initialize` | `inline void RTC_Initialize(void)` | verified |
| `RTC_IsPM` | `inline bool RTC_IsPM(void)` | verified |
| `RTC_IsSettingOK` | `inline bool RTC_IsSettingOK(void)` | verified |
| `RTC_LoadData` | `bool RTC_LoadData(u8* data)` | verified |
| `RTC_LoadDataSigned` | `bool RTC_LoadDataSigned(u8* data)` | verified_divergent |
| `RTC_Read` | `inline u8 RTC_Read(u8 reg)` | verified |
| `RTC_ReadRaw` | `void RTC_ReadRaw(u8 reg, u8* data, u8 num)` | verified |
| `RTC_SaveData` | `void RTC_SaveData(u8* data)` | verified |
| `RTC_SaveDataSigned` | `void RTC_SaveDataSigned(u8* data)` | verified_divergent |
| `RTC_Set24H` | `inline void RTC_Set24H(bool enable)` | verified |
| `RTC_SetAreaCode` | `inline void RTC_SetAreaCode(u8 code)` | verified |
| `RTC_SetMode` | `inline void RTC_SetMode(u8 mode)` | verified |
| `RTC_Write` | `inline void RTC_Write(u8 reg, u8 value)` | verified |
| `RTC_WriteRaw` | `void RTC_WriteRaw(u8 reg, u8* data, u8 num)` | verified |

## compress (1 functions)

| function | signature | status |
|---|---|---|
| `RLEp_UnpackToRAM` | `u16 RLEp_UnpackToRAM(const u8* src, u8* dst RLEP_FIXSIZE_PARAM)` | verified |

## crypt (5 functions)

| function | signature | status |
|---|---|---|
| `Crypt_Decode` | `bool Crypt_Decode(const c8* str, void* data)` | verified_divergent |
| `Crypt_Encode` | `bool Crypt_Encode(const void* data, u8 size, c8* str)` | verified_divergent |
| `Crypt_SetCode` | `inline void Crypt_SetCode(const u16* code)` | verified_divergent |
| `Crypt_SetKey` | `inline void Crypt_SetKey(const c8* key)` | verified_divergent |
| `Crypt_SetMap` | `inline void Crypt_SetMap(const c8* map)` | verified_divergent |

## debug (10 functions)

| function | signature | status |
|---|---|---|
| `DEBUG_ASSERT` | `void DEBUG_ASSERT(bool a)` | verified_divergent |
| `DEBUG_BREAK` | `void DEBUG_BREAK()` | verified_divergent |
| `DEBUG_INIT` | `void DEBUG_INIT()` | verified_divergent |
| `DEBUG_LOG` | `void DEBUG_LOG(const c8* msg)` | verified_divergent |
| `DEBUG_LOGNUM` | `void DEBUG_LOGNUM(const c8* msg, u8 num)` | verified_divergent |
| `DEBUG_PRINT` | `void DEBUG_PRINT(const c8* format, ...)` | verified_divergent |
| `PROFILE_FRAME_END` | `inline void PROFILE_FRAME_END()` | verified_divergent |
| `PROFILE_FRAME_START` | `inline void PROFILE_FRAME_START()` | verified_divergent |
| `PROFILE_SECTION_END` | `inline void PROFILE_SECTION_END(u8 level, u8 section, const c8* msg)` | verified_divergent |
| `PROFILE_SECTION_START` | `inline void PROFILE_SECTION_START(u8 level, u8 section, const c8* msg)` | verified_divergent |

## dos (71 functions)

| function | signature | status |
|---|---|---|
| `DOS_AvailableDrives` | `DOS_AvailableDrives()` | verified |
| `DOS_Beep` | `inline void DOS_Beep()` | verified |
| `DOS_Call` | `void DOS_Call(u8 func)` | verified |
| `DOS_ChangeDirectory` | `u8 DOS_ChangeDirectory(const c8* path)` | verified |
| `DOS_CharInput` | `c8 DOS_CharInput()` | verified |
| `DOS_CharOutput` | `void DOS_CharOutput(c8 chr)` | verified |
| `DOS_ClearScreen` | `inline void DOS_ClearScreen()` | verified |
| `DOS_CloseFCB` | `u8 DOS_CloseFCB(DOS_FCB* stream)` | verified |
| `DOS_CloseHandle` | `u8 DOS_CloseHandle(u8 file)` | verified |
| `DOS_CreateFCB` | `u8 DOS_CreateFCB(DOS_FCB* stream)` | verified |
| `DOS_CreateHandle` | `u8 DOS_CreateHandle(const c8* path, u8 mode, u8 attr)` | verified |
| `DOS_Delete` | `u8 DOS_Delete(const c8* path)` | verified |
| `DOS_DeleteFCB` | `u8 DOS_DeleteFCB(DOS_FCB* stream)` | verified |
| `DOS_DeleteHandle` | `u8 DOS_DeleteHandle(u8 file)` | verified |
| `DOS_DuplicateHandle` | `u8 DOS_DuplicateHandle(u8 file)` | verified |
| `DOS_EnsureHandle` | `u8 DOS_EnsureHandle(u8 file)` | verified |
| `DOS_Exit` | `void DOS_Exit(u8 err)` | verified |
| `DOS_Exit0` | `void DOS_Exit0()` | verified |
| `DOS_Explain` | `void DOS_Explain(u8 err, c8* str)` | verified |
| `DOS_FindFirstFileFCB` | `u8 DOS_FindFirstFileFCB(DOS_FCB* stream)` | verified |
| `DOS_FindNextFileFCB` | `u8 DOS_FindNextFileFCB()` | verified |
| `DOS_GetAttribute` | `u8 DOS_GetAttribute(const c8* path)` | verified |
| `DOS_GetAttributeHandle` | `u8 DOS_GetAttributeHandle(u8 file)` | verified |
| `DOS_GetCurrentDiskDirectory` | `inline u8 DOS_GetCurrentDiskDirectory(const c8* path)` | verified |
| `DOS_GetCurrentDrive` | `u8 DOS_GetCurrentDrive()` | verified |
| `DOS_GetCurrentDriveLetter` | `inline c8 DOS_GetCurrentDriveLetter()` | verified |
| `DOS_GetDirectory` | `u8 DOS_GetDirectory(u8 drive, const c8* path)` | verified |
| `DOS_GetDiskInfo` | `void DOS_GetDiskInfo(u8 drive, DOS_DiskInfo* info)` | verified_divergent |
| `DOS_GetDiskParam` | `u8 DOS_GetDiskParam(u8 drv, DOS_DiskParam* param)` | verified |
| `DOS_GetFileDay` | `inline u8 DOS_GetFileDay(const DOS_FIB* fib)` | verified |
| `DOS_GetFileHour` | `inline u8 DOS_GetFileHour(const DOS_FIB* fib)` | verified |
| `DOS_GetFileMinute` | `inline u8 DOS_GetFileMinute(const DOS_FIB* fib)` | verified |
| `DOS_GetFileMonth` | `inline u8 DOS_GetFileMonth(const DOS_FIB* fib)` | verified |
| `DOS_GetFileSecond` | `inline u8 DOS_GetFileSecond(const DOS_FIB* fib)` | verified |
| `DOS_GetFileYear` | `inline u16 DOS_GetFileYear(const DOS_FIB* fib)` | verified |
| `DOS_GetFreeBytes` | `inline u32 DOS_GetFreeBytes(DOS_DiskInfo* info)` | verified |
| `DOS_GetFreeClusters` | `inline u16 DOS_GetFreeClusters(DOS_DiskInfo* info)` | verified |
| `DOS_GetFreeSectors` | `inline u32 DOS_GetFreeSectors(DOS_DiskInfo* info)` | verified |
| `DOS_GetFreeSpace` | `u16 DOS_GetFreeSpace(u8 drive)` | verified_divergent |
| `DOS_GetLastError` | `inline u8 DOS_GetLastError()` | verified |
| `DOS_GetSizeFCB` | `inline u32 DOS_GetSizeFCB(DOS_FCB* stream)` | verified |
| `DOS_GetTime` | `const DOS_Time* DOS_GetTime()` | verified_divergent |
| `DOS_GetTotalBytes` | `inline u32 DOS_GetTotalBytes(DOS_DiskInfo* info)` | verified |
| `DOS_GetTotalClusters` | `inline u16 DOS_GetTotalClusters(DOS_DiskInfo* info)` | verified |
| `DOS_GetTotalSectors` | `inline u32 DOS_GetTotalSectors(DOS_DiskInfo* info)` | verified |
| `DOS_GetVersion` | `u8 DOS_GetVersion(DOS_Version* ver)` | verified |
| `DOS_InstallErrorHandler` | `void DOS_InstallErrorHandler(callback cb, u8 size)` | verified_divergent |
| `DOS_InterSlotCall` | `void DOS_InterSlotCall(u8 slot, u16 addr)` | verified_divergent |
| `DOS_InterSlotRead` | `u8 DOS_InterSlotRead(u8 slot, u16 addr)` | verified_divergent |
| `DOS_InterSlotWrite` | `void DOS_InterSlotWrite(u8 slot, u16 addr, u8 value)` | verified_divergent |
| `DOS_Move` | `u8 DOS_Move(const c8* path, const c8* newPath)` | verified |
| `DOS_MoveHandle` | `u8 DOS_MoveHandle(u8 file, const c8* newPath)` | verified |
| `DOS_OpenFCB` | `u8 DOS_OpenFCB(DOS_FCB* stream)` | verified |
| `DOS_OpenHandle` | `u8 DOS_OpenHandle(const c8* path, u8 mode)` | verified |
| `DOS_RandomBlockReadFCB` | `u16 DOS_RandomBlockReadFCB(DOS_FCB* stream, u16 records)` | verified |
| `DOS_RandomBlockWriteFCB` | `u8 DOS_RandomBlockWriteFCB(DOS_FCB* stream, u16 records)` | verified |
| `DOS_ReadHandle` | `u16 DOS_ReadHandle(u8 file, void* buffer, u16 size)` | verified |
| `DOS_Rename` | `u8 DOS_Rename(const c8* path, const c8* newPath)` | verified |
| `DOS_RenameHandle` | `u8 DOS_RenameHandle(u8 file, const c8* newPath)` | verified |
| `DOS_ResetLastError` | `inline void DOS_ResetLastError()` | verified |
| `DOS_Return` | `inline void DOS_Return()` | verified |
| `DOS_SeekHandle` | `DOS_SeekHandle(u8 file, i32 offset, u8 mode)` | verified |
| `DOS_SelectDrive` | `u8 DOS_SelectDrive(u8 drive)` | verified |
| `DOS_SelectDriveLetter` | `inline u8 DOS_SelectDriveLetter(c8 drive)` | verified |
| `DOS_SequentialReadFCB` | `u8 DOS_SequentialReadFCB(DOS_FCB* stream)` | verified |
| `DOS_SequentialWriteFCB` | `u8 DOS_SequentialWriteFCB(DOS_FCB* stream)` | verified |
| `DOS_SetAttribute` | `u8 DOS_SetAttribute(const c8* path, u8 attr)` | verified |
| `DOS_SetAttributeHandle` | `u8 DOS_SetAttributeHandle(u8 file, u8 attr)` | verified |
| `DOS_SetTransferAddr` | `void DOS_SetTransferAddr(void* data)` | verified |
| `DOS_StringOutput` | `void DOS_StringOutput(const c8* str)` | verified |
| `DOS_WriteHandle` | `u16 DOS_WriteHandle(u8 file, const void* buffer, u16 size)` | verified |

## dos_mapper (15 functions)

| function | signature | status |
|---|---|---|
| `DOSMapper_Alloc` | `bool DOSMapper_Alloc(u8 type, u8 slot, DOS_Segment* seg)` | verified |
| `DOSMapper_Free` | `bool DOSMapper_Free(u8 seg, u8 slot)` | verified |
| `DOSMapper_FreeStruct` | `inline bool DOSMapper_FreeStruct(DOS_Segment* seg)` | verified |
| `DOSMapper_GetPage` | `u8 DOSMapper_GetPage(u8 page)` | verified |
| `DOSMapper_GetPage0` | `u8 DOSMapper_GetPage0()` | verified |
| `DOSMapper_GetPage1` | `u8 DOSMapper_GetPage1()` | verified |
| `DOSMapper_GetPage2` | `u8 DOSMapper_GetPage2()` | verified |
| `DOSMapper_GetPage3` | `u8 DOSMapper_GetPage3()` | verified |
| `DOSMapper_Init` | `bool DOSMapper_Init()` | verified |
| `DOSMapper_ReadByte` | `u8 DOSMapper_ReadByte(u8 seg, u16 addr)` | verified |
| `DOSMapper_SetPage` | `void DOSMapper_SetPage(u8 page, u8 seg)` | verified |
| `DOSMapper_SetPage0` | `void DOSMapper_SetPage0(u8 seg)` | verified |
| `DOSMapper_SetPage1` | `void DOSMapper_SetPage1(u8 seg)` | verified |
| `DOSMapper_SetPage2` | `void DOSMapper_SetPage2(u8 seg)` | verified |
| `DOSMapper_WriteByte` | `void DOSMapper_WriteByte(u8 seg, u16 addr, u8 val)` | verified |

## draw (7 functions)

| function | signature | status |
|---|---|---|
| `Draw_Box` | `void Draw_Box(UX x1, UY y1, UX x2, UY y2, u8 color, u8 op)` | verified |
| `Draw_Circle` | `void Draw_Circle(UX x, UY y, u8 radius, u8 color, u8 op)` | verified |
| `Draw_FillBox` | `void Draw_FillBox(UX x1, UY y1, UX x2, UY y2, u8 color, u8 op)` | verified |
| `Draw_Line` | `void Draw_Line(UX x1, UY y1, UX x2, UY y2, u8 color, u8 op)` | verified |
| `Draw_LineH` | `void Draw_LineH(UX x1, UX x2, UY y, u8 color, u8 op)` | verified |
| `Draw_LineV` | `void Draw_LineV(UX x, UY y1, UY y2, u8 color, u8 op)` | verified |
| `Draw_Point` | `void Draw_Point(UX x, UY y, u8 color, u8 op)` | verified |

## extbios (1 functions)

| function | signature | status |
|---|---|---|
| `ExtBIOS_Check` | `inline bool ExtBIOS_Check()` | verified |

## fixed_point (4 functions)

| function | signature | status |
|---|---|---|
| `QMN_Get16` | `i16 QMN_Get16(u16 q, i16 a)` | verified |
| `QMN_Get8` | `i8 QMN_Get8(u16 q, i8 a)` | verified |
| `QMN_Set16` | `i16 QMN_Set16(u16 q, i16 a)` | verified |
| `QMN_Set8` | `i8 QMN_Set8(u16 q, i8 a)` | verified |

## fsm (4 functions)

| function | signature | status |
|---|---|---|
| `FSM_IsPending` | `inline bool FSM_IsPending()` | verified |
| `FSM_SetPrevious` | `inline void FSM_SetPrevious()` | verified |
| `FSM_SetState` | `void FSM_SetState(FSM_State* state)` | verified |
| `FSM_Update` | `void FSM_Update()` | verified |

## game_pawn (15 functions)

| function | signature | status |
|---|---|---|
| `GamePawn_Disable` | `inline void GamePawn_Disable(Game_Pawn* pawn)` | verified |
| `GamePawn_Draw` | `void GamePawn_Draw(Game_Pawn* pawn)` | verified |
| `GamePawn_Enable` | `inline void GamePawn_Enable(Game_Pawn* pawn)` | verified |
| `GamePawn_GetCallbackCellX` | `inline u8 GamePawn_GetCallbackCellX()` | verified_divergent |
| `GamePawn_GetCallbackCellY` | `inline u8 GamePawn_GetCallbackCellY()` | verified_divergent |
| `GamePawn_GetPhysicsState` | `inline u8 GamePawn_GetPhysicsState(Game_Pawn* pawn)` | verified |
| `GamePawn_Initialize` | `void GamePawn_Initialize(Game_Pawn* pawn, const Game_Sprite* sprtList, u8 sprtNum, u8 sprtID, const Game_Action* actList)` | verified |
| `GamePawn_InitializePhysics` | `void GamePawn_InitializePhysics(Game_Pawn* pawn, Game_PhysicsCB pcb, Game_CollisionCB ccb, u8 boundX, u8 boundY)` | verified |
| `GamePawn_SetAction` | `void GamePawn_SetAction(Game_Pawn* pawn, u8 id)` | verified |
| `GamePawn_SetEnable` | `void GamePawn_SetEnable(Game_Pawn* pawn, bool enable)` | verified |
| `GamePawn_SetMovement` | `void GamePawn_SetMovement(Game_Pawn* pawn, i8 dx, i8 dy)` | verified |
| `GamePawn_SetPosition` | `void GamePawn_SetPosition(Game_Pawn* pawn, u8 x, u8 y)` | verified |
| `GamePawn_SetTargetPosition` | `inline void GamePawn_SetTargetPosition(Game_Pawn* pawn, u8 x, u8 y)` | verified |
| `GamePawn_SetTileMap` | `inline void GamePawn_SetTileMap(const u8* map)` | verified |
| `GamePawn_Update` | `void GamePawn_Update(Game_Pawn* pawn)` | verified |

## input (20 functions)

| function | signature | status |
|---|---|---|
| `Input_Detect` | `u8 Input_Detect(enum INPUT_PORT port)` | verified |
| `Joystick_GetDirection` | `inline u8 Joystick_GetDirection(u8 port)` | verified |
| `Joystick_GetDirectionChange` | `inline u8 Joystick_GetDirectionChange(u8 port)` | verified |
| `Joystick_IsButtonPressed` | `inline bool Joystick_IsButtonPressed(u8 port, u8 trigger)` | verified |
| `Joystick_IsButtonPushed` | `inline bool Joystick_IsButtonPushed(u8 port, u8 trigger)` | verified |
| `Joystick_Read` | `u8 Joystick_Read(u8 port) __FASTCALL __PRESERVES(b, c, d, e, h, iyl, iyh)` | verified |
| `Joystick_Update` | `void Joystick_Update()` | verified |
| `Keyboard_IsKeyPressed` | `inline bool Keyboard_IsKeyPressed(u8 key)` | verified |
| `Keyboard_IsKeyPushed` | `bool Keyboard_IsKeyPushed(u8 key)` | verified |
| `Keyboard_Read` | `u8 Keyboard_Read(u8 row) __FASTCALL __PRESERVES(b, c, d, e, h, iyl, iyh)` | verified |
| `Keyboard_ReadAsJoystick` | `u8 Keyboard_ReadAsJoystick()` | verified |
| `Keyboard_SetBuffer` | `inline void Keyboard_SetBuffer(u8* new, u8* old)` | verified |
| `Keyboard_Update` | `void Keyboard_Update()` | verified |
| `Mouse_GetAdjustedOffsetX` | `inline i8 Mouse_GetAdjustedOffsetX(Mouse_State* data, u8 spd)` | verified |
| `Mouse_GetAdjustedOffsetY` | `inline i8 Mouse_GetAdjustedOffsetY(Mouse_State* data, u8 spd)` | verified |
| `Mouse_GetOffsetX` | `inline i8 Mouse_GetOffsetX(Mouse_State* data)` | verified |
| `Mouse_GetOffsetY` | `inline i8 Mouse_GetOffsetY(Mouse_State* data)` | verified |
| `Mouse_IsButtonClick` | `inline bool Mouse_IsButtonClick(Mouse_State* data, u8 btn)` | verified |
| `Mouse_IsButtonPress` | `inline bool Mouse_IsButtonPress(Mouse_State* data, u8 btn)` | verified |
| `Mouse_Read` | `void Mouse_Read(u8 port, Mouse_State* data)` | verified |

## input_manager (9 functions)

| function | signature | status |
|---|---|---|
| `IPM_GetEventName` | `const c8* IPM_GetEventName(u8 ev)` | verified |
| `IPM_GetInputState` | `inline u8 IPM_GetInputState(u8 dev, u8 input)` | verified |
| `IPM_GetInputTimer` | `inline u8 IPM_GetInputTimer(u8 dev, u8 input)` | verified |
| `IPM_GetStatus` | `inline u8 IPM_GetStatus(u8 dev)` | verified |
| `IPM_GetStickDirection` | `u8 IPM_GetStickDirection(u8 dev)` | verified |
| `IPM_Initialize` | `void IPM_Initialize(IPM_Config* config)` | verified |
| `IPM_RegisterEvent` | `bool IPM_RegisterEvent(u8 dev, u8 input, u8 event, IPM_cb cb)` | verified |
| `IPM_SetTimer` | `inline void IPM_SetTimer(u8 doubleClk, u8 hold)` | verified |
| `IPM_Update` | `void IPM_Update()` | verified_divergent |

## localize (3 functions)

| function | signature | status |
|---|---|---|
| `Loc_GetText` | `inline const c8* Loc_GetText(u16 text)` | verified |
| `Loc_Initialize` | `void Loc_Initialize(const void* data, u8 num)` | verified |
| `Loc_SetLanguage` | `inline void Loc_SetLanguage(u8 lang)` | verified |

## math (25 functions)

| function | signature | status |
|---|---|---|
| `Math_Abs` | `inline u8 Math_Abs(i8 val)` | verified |
| `Math_Abs_16b` | `inline u16 Math_Abs_16b(i16 val)` | verified |
| `Math_Abs_32b` | `inline u32 Math_Abs_32b(i32 val)` | verified |
| `Math_Div10` | `i8 Math_Div10(i8 val) __FASTCALL __PRESERVES(a, b, c, iyl, iyh)` | verified |
| `Math_Div10_16b` | `i16 Math_Div10_16b(i16 val) __FASTCALL __PRESERVES(b, d, e, iyl, iyh)` | verified |
| `Math_Flip` | `u8 Math_Flip(u8 val) __PRESERVES(c, d, e, h, l, iyl, iyh)` | verified |
| `Math_Flip_16b` | `u16 Math_Flip_16b(u16 val) __PRESERVES(c, iyl, iyh)` | verified |
| `Math_GetRandom16` | `define Math_GetRandom16() 0 #else // Initialize 16-bit random generator seed. void Math_SetRandomSeed16(u16 seed)` | verified |
| `Math_GetRandom8` | `define Math_GetRandom8() 0 #else // Initialize 8-bit random generator seed. void Math_SetRandomSeed8(u8 seed)` | verified_divergent |
| `Math_GetRandomMax16` | `inline u16 Math_GetRandomMax16(u16 max)` | verified |
| `Math_GetRandomMax8` | `inline u8 Math_GetRandomMax8(u8 max)` | verified_divergent |
| `Math_GetRandomRange16` | `inline u16 Math_GetRandomRange16(u16 min, u16 max)` | verified |
| `Math_GetRandomRange8` | `inline u8 Math_GetRandomRange8(u8 min, u8 max)` | verified_divergent |
| `Math_Mod10` | `u8 Math_Mod10(u8 val) __PRESERVES(b, c, d, e, iyl, iyh)` | verified |
| `Math_Mod10_16b` | `u8 Math_Mod10_16b(u16 val) __FASTCALL __PRESERVES(b, c, d, e, iyl, iyh)` | verified |
| `Math_Negative` | `inline i8 Math_Negative(i8 val)` | verified |
| `Math_Negative16` | `i16 Math_Negative16(i16 val) __FASTCALL __PRESERVES(b, c, d, e, iyl, iyh)` | verified |
| `Math_SetRandomSeed16` | `define Math_SetRandomSeed16(seed) #define Math_GetRandom16() 0 #else // Initialize 16-bit random generator seed. void Math_SetRandomSeed16(u16 seed)` | verified |
| `Math_SetRandomSeed8` | `define Math_SetRandomSeed8(seed) #define Math_GetRandom8() 0 #else // Initialize 8-bit random generator seed. void Math_SetRandomSeed8(u8 seed)` | verified_divergent |
| `Math_SignedDiv16` | `i8 Math_SignedDiv16(i8 val) __NAKED __PRESERVES(b, c, d, e, h, l, iyl, iyh)` | verified |
| `Math_SignedDiv2` | `i8 Math_SignedDiv2(i8 val) __NAKED __PRESERVES(b, c, d, e, h, l, iyl, iyh)` | verified |
| `Math_SignedDiv32` | `i8 Math_SignedDiv32(i8 val) __NAKED __PRESERVES(b, c, d, e, h, l, iyl, iyh)` | verified |
| `Math_SignedDiv4` | `i8 Math_SignedDiv4(i8 val) __NAKED __PRESERVES(b, c, d, e, h, l, iyl, iyh)` | verified |
| `Math_SignedDiv8` | `i8 Math_SignedDiv8(i8 val) __NAKED __PRESERVES(b, c, d, e, h, l, iyl, iyh)` | verified |
| `Math_Swap` | `u16 Math_Swap(u16 val) __PRESERVES(a, b, c, iyl, iyh)` | verified |

## memory (15 functions)

| function | signature | status |
|---|---|---|
| `Mem_Copy` | `void Mem_Copy(const void* src, void* dest, u16 size)` | verified |
| `Mem_Copy_16b` | `inline void Mem_Copy_16b(const void* src, void* dest, u16 size)` | verified |
| `Mem_DynamicFree` | `void Mem_DynamicFree(void* ptr)` | verified |
| `Mem_DynamicInitialize` | `void Mem_DynamicInitialize(void* base, u16 size)` | verified |
| `Mem_DynamicInitializeHeap` | `inline void Mem_DynamicInitializeHeap(u16 size)` | verified |
| `Mem_FastCopy` | `void Mem_FastCopy(const void* src, void* dest, u16 size)` | verified |
| `Mem_FastCopy_16b` | `inline void Mem_FastCopy_16b(const void* src, void* dest, u16 size)` | verified |
| `Mem_FastSet` | `void Mem_FastSet(u8 val, void* dest, u16 size)` | verified |
| `Mem_GetDynamicSize` | `inline u16 Mem_GetDynamicSize(void* ptr)` | verified |
| `Mem_GetHeapAddress` | `inline u16 Mem_GetHeapAddress()` | verified |
| `Mem_GetHeapSize` | `inline u16 Mem_GetHeapSize()` | verified |
| `Mem_GetStackAddress` | `u16 Mem_GetStackAddress()` | verified |
| `Mem_HeapFree` | `inline void Mem_HeapFree(u16 size)` | verified |
| `Mem_Set` | `void Mem_Set(u8 val, void* dest, u16 size)` | verified |
| `Mem_Set_16b` | `void Mem_Set_16b(u16 val, void* dest, u16 size)` | verified |

## memory_mapper (5 functions)

| function | signature | status |
|---|---|---|
| `MemMap_Initialize` | `void MemMap_Initialize(void)` | verified |
| `MemMap_SetPage0` | `void MemMap_SetPage0(u8 seg)` | verified |
| `MemMap_SetPage1` | `void MemMap_SetPage1(u8 seg)` | verified |
| `MemMap_SetPage2` | `void MemMap_SetPage2(u8 seg)` | verified |
| `MemMap_SetPage3` | `void MemMap_SetPage3(u8 seg)` | verified_divergent |

## msx-audio (6 functions)

| function | signature | status |
|---|---|---|
| `MSXAudio_Detect` | `bool MSXAudio_Detect(void)` | verified |
| `MSXAudio_GetRegister` | `u8 MSXAudio_GetRegister(u8 reg)` | verified |
| `MSXAudio_Initialize` | `void MSXAudio_Initialize(void)` | verified |
| `MSXAudio_Mute` | `void MSXAudio_Mute(void)` | verified |
| `MSXAudio_Resume` | `void MSXAudio_Resume(void)` | verified |
| `MSXAudio_SetRegister` | `void MSXAudio_SetRegister(u8 reg, u8 value)` | verified |

## msx-music (7 functions)

| function | signature | status |
|---|---|---|
| `MSXMusic_Detect` | `u8 MSXMusic_Detect()` | verified |
| `MSXMusic_GetRegister` | `u8 MSXMusic_GetRegister(u8 reg)` | verified |
| `MSXMusic_GetSlotId` | `inline u8 MSXMusic_GetSlotId()` | verified |
| `MSXMusic_Initialize` | `u8 MSXMusic_Initialize()` | verified |
| `MSXMusic_Mute` | `void MSXMusic_Mute()` | verified_divergent |
| `MSXMusic_Resume` | `void MSXMusic_Resume()` | verified_divergent |
| `MSXMusic_SetRegister` | `void MSXMusic_SetRegister(u8 reg, u8 value)` | verified |

## mutex (5 functions)

| function | signature | status |
|---|---|---|
| `Mutex_Gate` | `inline bool Mutex_Gate(u8 mutex)` | verified |
| `Mutex_Init` | `inline void Mutex_Init(void)` | verified |
| `Mutex_Lock` | `inline void Mutex_Lock(u8 mutex)` | verified |
| `Mutex_Release` | `inline void Mutex_Release(u8 mutex)` | verified |
| `Mutex_Wait` | `inline void Mutex_Wait(u8 mutex)` | verified |

## print (58 functions)

| function | signature | status |
|---|---|---|
| `Print_Backspace` | `void Print_Backspace(u8 num)` | verified |
| `Print_Clear` | `void Print_Clear()` | verified |
| `Print_DrawBin8` | `void Print_DrawBin8(u8 value)` | verified |
| `Print_DrawBin8At` | `inline void Print_DrawBin8At(UX x, UY y, u8 val)` | verified |
| `Print_DrawBox` | `void Print_DrawBox(UX x, UY y, u8 width, u8 height)` | verified |
| `Print_DrawChar` | `void Print_DrawChar(u8 chr)` | verified |
| `Print_DrawCharAt` | `inline void Print_DrawCharAt(UX x, UY y, c8 chr)` | verified |
| `Print_DrawCharX` | `void Print_DrawCharX(c8 chr, u8 num)` | verified |
| `Print_DrawCharXAt` | `inline void Print_DrawCharXAt(UX x, UY y, c8 chr, u8 len)` | verified |
| `Print_DrawCharYAt` | `inline void Print_DrawCharYAt(UX x, UY y, c8 chr, u8 len)` | verified |
| `Print_DrawFormat` | `void Print_DrawFormat(const c8* format, ...)` | verified |
| `Print_DrawHex16` | `void Print_DrawHex16(u16 value)` | verified |
| `Print_DrawHex16At` | `inline void Print_DrawHex16At(UX x, UY y, u16 val)` | verified |
| `Print_DrawHex32` | `void Print_DrawHex32(u32 value)` | verified |
| `Print_DrawHex8` | `void Print_DrawHex8(u8 value)` | verified |
| `Print_DrawHex8At` | `inline void Print_DrawHex8At(UX x, UY y, u8 val)` | verified |
| `Print_DrawInt` | `void Print_DrawInt(i32 value)` | verified |
| `Print_DrawIntAt` | `inline void Print_DrawIntAt(UX x, UY y, i16 val)` | verified |
| `Print_DrawLineH` | `void Print_DrawLineH(UX x, UY y, u8 len)` | verified |
| `Print_DrawLineV` | `void Print_DrawLineV(UX x, UY y, u8 len)` | verified |
| `Print_DrawText` | `void Print_DrawText(const c8* string)` | verified |
| `Print_DrawTextAlign` | `inline void Print_DrawTextAlign(const c8* str, u8 align)` | verified |
| `Print_DrawTextAlignAt` | `inline void Print_DrawTextAlignAt(UX x, UY y, const c8* str, u8 align)` | verified |
| `Print_DrawTextAt` | `inline void Print_DrawTextAt(UX x, UY y, const c8* str)` | verified |
| `Print_DrawTextAtV` | `inline void Print_DrawTextAtV(UX x, UY y, const c8* str)` | verified |
| `Print_DrawTextOutline` | `void Print_DrawTextOutline(const c8* string, u8 color)` | verified_divergent |
| `Print_DrawTextShadow` | `void Print_DrawTextShadow(const c8* string, i8 offsetX, i8 offsetY, u8 color)` | verified_divergent |
| `Print_EnableOutline` | `void Print_EnableOutline(bool enable)` | verified |
| `Print_EnableShadow` | `void Print_EnableShadow(bool enable)` | verified |
| `Print_GetFontInfo` | `inline const Print_Data* Print_GetFontInfo()` | verified |
| `Print_GetPatternOffset` | `inline u8 Print_GetPatternOffset()` | verified |
| `Print_GetSpriteID` | `inline u8 Print_GetSpriteID()` | verified |
| `Print_GetSpritePattern` | `inline u8 Print_GetSpritePattern()` | verified |
| `Print_Initialize` | `bool Print_Initialize()` | verified |
| `Print_Return` | `inline void Print_Return()` | verified |
| `Print_SelectTextFont` | `inline void Print_SelectTextFont(const u8* font, u8 offset)` | verified |
| `Print_SetBitmapFont` | `void Print_SetBitmapFont(const u8* font)` | verified |
| `Print_SetCharSize` | `inline void Print_SetCharSize(u8 x, u8 y)` | verified |
| `Print_SetColor` | `void Print_SetColor(u8 text, u8 bg)` | verified |
| `Print_SetColorShade` | `void Print_SetColorShade(const u8* shade)` | verified_divergent |
| `Print_SetDirection` | `inline void Print_SetDirection(u8 dir)` | verified |
| `Print_SetFont` | `void Print_SetFont(const u8* font)` | verified |
| `Print_SetFontData` | `inline void Print_SetFontData(Print_Data* data)` | verified_divergent |
| `Print_SetFontEx` | `inline void Print_SetFontEx(u8 patternX, u8 patternY, u8 sizeX, u8 sizeY, u8 firstChr, u8 lastChr, const u8* patterns)` | verified |
| `Print_SetMode` | `void Print_SetMode(u8 mode)` | verified |
| `Print_SetOutline` | `void Print_SetOutline(bool enable, u8 color)` | verified |
| `Print_SetPatternOffset` | `inline void Print_SetPatternOffset(u8 offset)` | verified |
| `Print_SetPosition` | `inline void Print_SetPosition(UX x, UY y)` | verified |
| `Print_SetPositionX` | `inline void Print_SetPositionX(UX x)` | verified |
| `Print_SetPositionY` | `inline void Print_SetPositionY(UY y)` | verified |
| `Print_SetShadow` | `void Print_SetShadow(bool enable, i8 offsetX, i8 offsetY, u8 color)` | verified |
| `Print_SetSpriteFont` | `void Print_SetSpriteFont(const u8* font, u8 patIdx, u8 sprtIdx)` | verified |
| `Print_SetSpriteID` | `inline void Print_SetSpriteID(u8 id)` | verified |
| `Print_SetTabSize` | `inline void Print_SetTabSize(u8 size)` | verified |
| `Print_SetTextFont` | `void Print_SetTextFont(const u8* font, u8 offset)` | verified |
| `Print_SetVRAMFont` | `void Print_SetVRAMFont(const u8* font, UY y, u8 color, bool trans)` | verified |
| `Print_Space` | `inline void Print_Space()` | verified |
| `Print_Tab` | `inline void Print_Tab()` | verified |

## psg (14 functions)

| function | signature | status |
|---|---|---|
| `PSG_Apply` | `void PSG_Apply()` | verified |
| `PSG_EnableEnvelope` | `void PSG_EnableEnvelope(u8 chan, bool val)` | verified |
| `PSG_EnableNoise` | `void PSG_EnableNoise(u8 chan, bool val)` | verified |
| `PSG_EnableTone` | `void PSG_EnableTone(u8 chan, bool val)` | verified |
| `PSG_GetRegister` | `u8 PSG_GetRegister(u8 reg)` | verified |
| `PSG_Mute` | `void PSG_Mute()` | verified |
| `PSG_Resume` | `void PSG_Resume()` | verified |
| `PSG_SetEnvelope` | `void PSG_SetEnvelope(u16 period)` | verified |
| `PSG_SetMixer` | `void PSG_SetMixer(u8 mix)` | verified |
| `PSG_SetNoise` | `void PSG_SetNoise(u8 period)` | verified |
| `PSG_SetRegister` | `void PSG_SetRegister(u8 reg, u8 value)` | verified |
| `PSG_SetShape` | `void PSG_SetShape(u8 shape)` | verified |
| `PSG_SetTone` | `void PSG_SetTone(u8 chan, u16 period)` | verified |
| `PSG_SetVolume` | `void PSG_SetVolume(u8 chan, u8 vol)` | verified |

## rom_mapper (10 functions)

| function | signature | status |
|---|---|---|
| `GET_BANK_SEGMENT` | `u8 GET_BANK_SEGMENT(u8 b)` | verified_divergent |
| `GET_BANK_SEGMENT_HIGH` | `u8 GET_BANK_SEGMENT_HIGH(u8 b)` | verified_divergent |
| `GET_BANK_SEGMENT_LOW` | `u8 GET_BANK_SEGMENT_LOW(u8 b)` | verified_divergent |
| `SET_BANK_SEGMENT` | `void SET_BANK_SEGMENT(u8 b, u8 s)` | verified_divergent |
| `SET_BANK_SEGMENT_HIGH` | `void SET_BANK_SEGMENT_HIGH(u8 b, u8 s)` | verified_divergent |
| `SET_BANK_SEGMENT_LOW` | `void SET_BANK_SEGMENT_LOW(u8 b, u8 s)` | verified_divergent |
| `YAMANOOTO_ENABLE` | `inline void YAMANOOTO_ENABLE(bool enable)` | verified_divergent |
| `YAMANOOTO_SET_CFGR` | `inline void YAMANOOTO_SET_CFGR(u8 value)` | verified_divergent |
| `YAMANOOTO_SET_ENAR` | `inline void YAMANOOTO_SET_ENAR(u8 value)` | verified_divergent |
| `YAMANOOTO_SET_OFFR` | `inline void YAMANOOTO_SET_OFFR(u8 value)` | verified_divergent |

## scc (11 functions)

| function | signature | status |
|---|---|---|
| `SCC_GetRegister` | `u8 SCC_GetRegister(u8 reg)` | verified |
| `SCC_Initialize` | `bool SCC_Initialize()` | verified |
| `SCC_LoadWaveform` | `void SCC_LoadWaveform(u8 channel, const u8* data)` | verified |
| `SCC_Mute` | `void SCC_Mute()` | verified |
| `SCC_Resume` | `void SCC_Resume()` | verified |
| `SCC_Select` | `void SCC_Select()` | verified |
| `SCC_SetFrequency` | `void SCC_SetFrequency(u8 channel, u16 freq)` | verified |
| `SCC_SetMixer` | `inline void SCC_SetMixer(u8 mix)` | verified |
| `SCC_SetRegister` | `void SCC_SetRegister(u8 reg, u8 value)` | verified |
| `SCC_SetSlot` | `inline void SCC_SetSlot(u8 slotId)` | verified |
| `SCC_SetVolume` | `inline void SCC_SetVolume(u8 channel, u8 vol)` | verified |

## scroll (5 functions)

| function | signature | status |
|---|---|---|
| `Scroll_HBlankAdjust` | `void Scroll_HBlankAdjust(u8 adjust)` | verified |
| `Scroll_Initialize` | `u8 Scroll_Initialize(u16 map)` | verified |
| `Scroll_SetOffsetH` | `void Scroll_SetOffsetH(i8 offset)` | verified_divergent |
| `Scroll_SetOffsetV` | `void Scroll_SetOffsetV(i8 offset)` | verified |
| `Scroll_Update` | `void Scroll_Update()` | verified_divergent |

## sprite_fx (20 functions)

| function | signature | status |
|---|---|---|
| `SpriteFX_CropBottom16` | `void SpriteFX_CropBottom16(const u8* src, u8* dest, u8 offset)` | verified |
| `SpriteFX_CropBottom8` | `void SpriteFX_CropBottom8(const u8* src, u8* dest, u8 offset)` | verified |
| `SpriteFX_CropLeft16` | `void SpriteFX_CropLeft16(const u8* src, u8* dest, u8 offset)` | verified |
| `SpriteFX_CropLeft8` | `void SpriteFX_CropLeft8(const u8* src, u8* dest, u8 offset)` | verified |
| `SpriteFX_CropRight16` | `void SpriteFX_CropRight16(const u8* src, u8* dest, u8 offset)` | verified |
| `SpriteFX_CropRight8` | `void SpriteFX_CropRight8(const u8* src, u8* dest, u8 offset)` | verified |
| `SpriteFX_CropTop16` | `void SpriteFX_CropTop16(const u8* src, u8* dest, u8 offset)` | verified |
| `SpriteFX_CropTop8` | `void SpriteFX_CropTop8(const u8* src, u8* dest, u8 offset)` | verified |
| `SpriteFX_FlipHorizontal16` | `void SpriteFX_FlipHorizontal16(const u8* src, u8* dest) __PRESERVES(iyl, iyh)` | verified |
| `SpriteFX_FlipHorizontal8` | `void SpriteFX_FlipHorizontal8(const u8* src, u8* dest) __PRESERVES(iyl, iyh)` | verified |
| `SpriteFX_FlipVertical16` | `void SpriteFX_FlipVertical16(const u8* src, u8* dest) __PRESERVES(iyl, iyh)` | verified |
| `SpriteFX_FlipVertical8` | `void SpriteFX_FlipVertical8(const u8* src, u8* dest) __PRESERVES(iyl, iyh)` | verified |
| `SpriteFX_Mask16` | `void SpriteFX_Mask16(const u8* src, u8* dest, const u8* mask)` | verified |
| `SpriteFX_Mask8` | `void SpriteFX_Mask8(const u8* src, u8* dest, const u8* mask)` | verified |
| `SpriteFX_RotateHalfTurn16` | `void SpriteFX_RotateHalfTurn16(const u8* src, u8* dest)` | verified |
| `SpriteFX_RotateHalfTurn8` | `void SpriteFX_RotateHalfTurn8(const u8* src, u8* dest) __PRESERVES(iyl, iyh)` | verified |
| `SpriteFX_RotateLeft16` | `void SpriteFX_RotateLeft16(const u8* src, u8* dest)` | verified |
| `SpriteFX_RotateLeft8` | `void SpriteFX_RotateLeft8(const u8* src, u8* dest) __PRESERVES(iyl, iyh)` | verified |
| `SpriteFX_RotateRight16` | `void SpriteFX_RotateRight16(const u8* src, u8* dest)` | verified |
| `SpriteFX_RotateRight8` | `void SpriteFX_RotateRight8(const u8* src, u8* dest) __PRESERVES(iyl, iyh)` | verified |

## string (10 functions)

| function | signature | status |
|---|---|---|
| `Char_IsAlpha` | `inline bool Char_IsAlpha(c8 chr)` | verified |
| `Char_IsAlphaNum` | `inline bool Char_IsAlphaNum(c8 chr)` | verified |
| `Char_IsNum` | `inline bool Char_IsNum(c8 chr)` | verified |
| `String_Copy` | `inline void String_Copy(c8* dst, const c8* src)` | verified |
| `String_Format` | `void String_Format(c8* dest, const c8* format, ...)` | verified |
| `String_FromUInt16` | `void String_FromUInt16(u16 value, c8* string)` | verified |
| `String_FromUInt16ZT` | `void String_FromUInt16ZT(u16 value, c8* string)` | verified |
| `String_FromUInt8` | `void String_FromUInt8(u8 value, c8* string)` | verified |
| `String_FromUInt8ZT` | `void String_FromUInt8ZT(u8 value, c8* string)` | verified |
| `String_Length` | `inline u8 String_Length(const c8* str)` | verified |

## system (34 functions)

| function | signature | status |
|---|---|---|
| `Call` | `inline void Call(u16 addr)` | verified |
| `CallA` | `inline void CallA(u16 addr, u8 a)` | verified |
| `CallAToA` | `inline u8 CallAToA(u16 addr, u8 a)` | verified |
| `CallDriver` | `inline u8 CallDriver(u16 addr, u8 val)` | verified |
| `CallHL` | `inline void CallHL(u16 addr, u16 hl)` | verified |
| `CallHLToA` | `inline u8 CallHLToA(u16 addr, u16 hl)` | verified |
| `CallL` | `inline void CallL(u16 addr, u8 l)` | verified |
| `CallLToA` | `inline u8 CallLToA(u16 addr, u8 l)` | verified_divergent |
| `CallToA` | `inline u8 CallToA(u16 addr)` | verified |
| `DisableInterrupt` | `inline void DisableInterrupt()` | verified |
| `EnableInterrupt` | `inline void EnableInterrupt()` | verified |
| `Halt` | `inline void Halt()` | verified |
| `Peek` | `inline u8 Peek(u16 addr)` | verified |
| `Peek16` | `inline u16 Peek16(u16 addr)` | verified |
| `Poke` | `inline void Poke(u16 addr, u8 val)` | verified |
| `Poke16` | `inline void Poke16(u16 addr, u16 val)` | verified |
| `Sys_CheckSlot` | `u8 Sys_CheckSlot(CheckSlotCallback cb)` | verified |
| `Sys_GetBIOSInfo` | `inline u8 Sys_GetBIOSInfo()` | verified |
| `Sys_GetFirstAddr` | `inline u16 Sys_GetFirstAddr()` | verified_divergent |
| `Sys_GetFrequency` | `inline u8 Sys_GetFrequency()` | verified |
| `Sys_GetHeaderAddr` | `inline u16 Sys_GetHeaderAddr()` | verified_divergent |
| `Sys_GetLastAddr` | `inline u16 Sys_GetLastAddr()` | verified_divergent |
| `Sys_GetMSXVersion` | `inline u8 Sys_GetMSXVersion()` | verified |
| `Sys_GetPageSlot` | `u8 Sys_GetPageSlot(u8 page)` | verified |
| `Sys_Is50Hz` | `inline bool Sys_Is50Hz()` | verified |
| `Sys_Is60Hz` | `inline bool Sys_Is60Hz()` | verified |
| `Sys_IsSlotExpanded` | `inline bool Sys_IsSlotExpanded(u8 slot)` | verified |
| `Sys_PlayClickSound` | `inline void Sys_PlayClickSound()` | verified |
| `Sys_SetPage0Slot` | `void Sys_SetPage0Slot(u8 slotId)` | verified |
| `Sys_SetPageSlot` | `void Sys_SetPageSlot(u8 page, u8 slotId)` | verified |
| `Sys_SlotGetPrimary` | `inline u8 Sys_SlotGetPrimary(u8 slotId)` | verified |
| `Sys_SlotGetSecondary` | `inline u8 Sys_SlotGetSecondary(u8 slotId)` | verified |
| `Sys_SlotIsExpended` | `inline bool Sys_SlotIsExpended(u8 slotId)` | verified |
| `Sys_StopClickSound` | `inline void Sys_StopClickSound()` | verified |

## tile (14 functions)

| function | signature | status |
|---|---|---|
| `Tile_DrawBlock` | `inline void Tile_DrawBlock(u8 dx, u8 dy, u8 sx, u8 sy, u8 width, u8 height)` | verified |
| `Tile_DrawMapChunk` | `void Tile_DrawMapChunk(u8 x, u8 y, const u8* map, u8 width, u8 height)` | verified |
| `Tile_DrawScreen` | `void Tile_DrawScreen(const u8* map)` | verified |
| `Tile_DrawTile` | `inline void Tile_DrawTile(u8 x, u8 y, u8 tile)` | verified |
| `Tile_FillBank` | `inline void Tile_FillBank(u8 bank, u8 value)` | verified |
| `Tile_FillScreen` | `inline void Tile_FillScreen(u8 color)` | verified |
| `Tile_FillTile` | `inline void Tile_FillTile(u8 x, u8 y, u8 color)` | verified |
| `Tile_GetBankAddress` | `inline u32 Tile_GetBankAddress(u8 bank)` | verified |
| `Tile_GetBankAddressEx` | `inline u32 Tile_GetBankAddressEx(u8 bank, u16 offset)` | verified |
| `Tile_LoadBank` | `inline void Tile_LoadBank(u8 bank, const u8* data, u16 num)` | verified |
| `Tile_LoadBankEx` | `void Tile_LoadBankEx(u8 bank, const u8* data, u16 offset, u16 num)` | verified |
| `Tile_SelectBank` | `inline void Tile_SelectBank(u8 bank)` | verified |
| `Tile_SetBankPage` | `inline void Tile_SetBankPage(u8 page)` | verified |
| `Tile_SetDrawPage` | `inline void Tile_SetDrawPage(u8 page)` | verified |

## v9990 (125 functions)

| function | signature | status |
|---|---|---|
| `GetCommandBX` | `inline u16 GetCommandBX()` | verified |
| `V9_ClearVRAM` | `void V9_ClearVRAM() __PRESERVES(d, e, h, l, iyl, iyh)` | verified |
| `V9_CommandADVANCE` | `inline void V9_CommandADVANCE(u16 dx, u16 dy, u8 shift)` | verified_divergent |
| `V9_CommandBMLL` | `inline void V9_CommandBMLL(u32 sa, u32 da, u32 na, u8 arg)` | verified |
| `V9_CommandBMLX` | `inline void V9_CommandBMLX(u16 sx, u16 sy, u32 da, u16 nx, u16 ny, u8 arg)` | verified |
| `V9_CommandBMXL` | `inline void V9_CommandBMXL(u32 sa, u16 dx, u16 dy, u16 nx, u16 ny, u8 arg)` | verified |
| `V9_CommandCMMC` | `inline void V9_CommandCMMC(u16 dx, u16 dy, u16 nx, u16 ny, u8 arg, u16 fc, u16 bc)` | verified |
| `V9_CommandCMMM` | `inline void V9_CommandCMMM(u32 sa, u16 dx, u16 dy, u16 nx, u16 ny, u8 arg, u16 fc, u16 bc)` | verified |
| `V9_CommandLINE` | `inline void V9_CommandLINE(u16 dx, u16 dy, u16 mj, u16 mi, u8 arg, u16 fc)` | verified |
| `V9_CommandLMMM` | `inline void V9_CommandLMMM(u16 sx, u16 sy, u16 dx, u16 dy, u16 nx, u16 ny, u8 arg)` | verified |
| `V9_CommandLMMV` | `inline void V9_CommandLMMV(u16 dx, u16 dy, u16 nx, u16 ny, u8 arg, u16 fc)` | verified |
| `V9_CommandPOINT` | `inline void V9_CommandPOINT(u16 sx, u16 sy)` | verified |
| `V9_CommandPSET` | `inline void V9_CommandPSET(u16 dx, u16 dy, u16 fc, u8 shift)` | verified |
| `V9_CommandSEARCH` | `inline void V9_CommandSEARCH(u16 sx, u16 sy, u8 arg, u16 fc)` | verified |
| `V9_CommandSTOP` | `inline void V9_CommandSTOP()` | verified |
| `V9_Detect` | `bool V9_Detect()` | verified |
| `V9_DisableInterrupt` | `inline void V9_DisableInterrupt()` | verified |
| `V9_ExecCommand` | `inline void V9_ExecCommand(u8 op)` | verified |
| `V9_FillVRAM` | `inline void V9_FillVRAM(u32 addr, u8 value, u16 count)` | verified |
| `V9_FillVRAM16` | `inline void V9_FillVRAM16(u32 addr, u16 value, u16 count)` | verified |
| `V9_FillVRAM16_CurrentAddr` | `void V9_FillVRAM16_CurrentAddr(u16 value, u16 count)` | verified |
| `V9_FillVRAM_CurrentAddr` | `void V9_FillVRAM_CurrentAddr(u8 value, u16 count)` | verified |
| `V9_GetBPP` | `inline u8 V9_GetBPP()` | verified |
| `V9_GetBackgroundColor` | `inline u8 V9_GetBackgroundColor()` | verified |
| `V9_GetImageSpaceWidth` | `inline u8 V9_GetImageSpaceWidth()` | verified |
| `V9_GetPort` | `u8 V9_GetPort(u8 port) __PRESERVES(b, d, e, h, l, iyl, iyh)` | verified |
| `V9_GetRegister` | `u8 V9_GetRegister(u8 reg) __PRESERVES(b, c, d, e, h, l, iyl, iyh)` | verified |
| `V9_GetStatus` | `inline u8 V9_GetStatus()` | verified |
| `V9_IsCmdComplete` | `inline bool V9_IsCmdComplete()` | verified |
| `V9_IsCmdDataReady` | `inline bool V9_IsCmdDataReady()` | verified |
| `V9_IsCmdRunning` | `inline bool V9_IsCmdRunning()` | verified |
| `V9_IsHBlank` | `inline bool V9_IsHBlank()` | verified |
| `V9_IsSecondField` | `inline bool V9_IsSecondField()` | verified |
| `V9_IsVBlank` | `inline bool V9_IsVBlank()` | verified |
| `V9_Peek` | `inline u8 V9_Peek(u32 addr)` | verified |
| `V9_Peek16` | `inline u16 V9_Peek16(u32 addr)` | verified |
| `V9_Peek16_CurrentAddr` | `u16 V9_Peek16_CurrentAddr() __PRESERVES(b, c, h, l, iyl, iyh)` | verified |
| `V9_Peek_CurrentAddr` | `u8 V9_Peek_CurrentAddr() __PRESERVES(b, c, d, e, h, l, iyl, iyh)` | verified |
| `V9_Poke` | `inline void V9_Poke(u32 addr, u8 val)` | verified |
| `V9_Poke16` | `inline void V9_Poke16(u32 addr, u16 val)` | verified |
| `V9_Poke16_CurrentAddr` | `void V9_Poke16_CurrentAddr(u16 val) __PRESERVES(b, c, d, e, iyl, iyh)` | verified |
| `V9_Poke_CurrentAddr` | `void V9_Poke_CurrentAddr(u8 val) __PRESERVES(b, c, d, e, h, l, iyl, iyh)` | verified |
| `V9_ReadVRAM` | `inline void V9_ReadVRAM(u32 addr, const u8* dest, u16 count)` | verified |
| `V9_ReadVRAM_CurrentAddr` | `void V9_ReadVRAM_CurrentAddr(const u8* dest, u16 count)` | verified |
| `V9_SelectPaletteBP2` | `inline void V9_SelectPaletteBP2(u8 a)` | verified |
| `V9_SelectPaletteBP4` | `inline void V9_SelectPaletteBP4(u8 a)` | verified |
| `V9_SelectPaletteP1` | `inline void V9_SelectPaletteP1(u8 a, u8 b)` | verified |
| `V9_SelectPaletteP2` | `inline void V9_SelectPaletteP2(u8 a)` | verified |
| `V9_SetAdjustOffset` | `inline void V9_SetAdjustOffset(u8 offset)` | verified |
| `V9_SetAdjustOffsetXY` | `inline void V9_SetAdjustOffsetXY(i8 x, i8 y)` | verified |
| `V9_SetBPP` | `inline void V9_SetBPP(u8 bpp)` | verified |
| `V9_SetBackgroundColor` | `inline void V9_SetBackgroundColor(u8 color)` | verified |
| `V9_SetCmdEndInterrupt` | `inline void V9_SetCmdEndInterrupt(bool enable)` | verified |
| `V9_SetColorMode` | `void V9_SetColorMode(u8 mode)` | verified |
| `V9_SetCommandArgument` | `inline void V9_SetCommandArgument(u8 arg)` | verified |
| `V9_SetCommandBC` | `inline void V9_SetCommandBC(u16 bc)` | verified |
| `V9_SetCommandDA` | `inline void V9_SetCommandDA(u32 da)` | verified |
| `V9_SetCommandDX` | `inline void V9_SetCommandDX(u16 dx)` | verified |
| `V9_SetCommandDY` | `inline void V9_SetCommandDY(u16 dy)` | verified |
| `V9_SetCommandFC` | `inline void V9_SetCommandFC(u16 fc)` | verified |
| `V9_SetCommandLogicalOp` | `inline void V9_SetCommandLogicalOp(u8 lop)` | verified |
| `V9_SetCommandMI` | `inline void V9_SetCommandMI(u16 mi)` | verified |
| `V9_SetCommandMJ` | `inline void V9_SetCommandMJ(u16 mj)` | verified |
| `V9_SetCommandNA` | `inline void V9_SetCommandNA(u32 na)` | verified |
| `V9_SetCommandNX` | `inline void V9_SetCommandNX(u16 nx)` | verified |
| `V9_SetCommandNY` | `inline void V9_SetCommandNY(u16 ny)` | verified |
| `V9_SetCommandSA` | `inline void V9_SetCommandSA(u32 sa)` | verified |
| `V9_SetCommandSX` | `inline void V9_SetCommandSX(u16 sx)` | verified |
| `V9_SetCommandSY` | `inline void V9_SetCommandSY(u16 sy)` | verified |
| `V9_SetCommandWriteMask` | `inline void V9_SetCommandWriteMask(u16 wm)` | verified |
| `V9_SetCursorAttribute` | `void V9_SetCursorAttribute(u8 id, u16 x, u16 y, u8 color)` | verified |
| `V9_SetCursorDisplay` | `void V9_SetCursorDisplay(u8 id, bool enable)` | verified |
| `V9_SetCursorEnable` | `inline void V9_SetCursorEnable(bool enable)` | verified |
| `V9_SetCursorPalette` | `inline void V9_SetCursorPalette(u8 offset)` | verified |
| `V9_SetCursorPattern` | `inline void V9_SetCursorPattern(u8 id, const u8* data)` | verified |
| `V9_SetDisplayEnable` | `inline void V9_SetDisplayEnable(bool enable)` | verified |
| `V9_SetFlag` | `inline void V9_SetFlag(u8 reg, u8 mask, u8 flag)` | verified |
| `V9_SetHBlankInterrupt` | `inline void V9_SetHBlankInterrupt(bool enable)` | verified_divergent |
| `V9_SetImageSpaceWidth` | `inline void V9_SetImageSpaceWidth(u8 width)` | verified |
| `V9_SetInterrupt` | `inline void V9_SetInterrupt(u8 flags)` | verified |
| `V9_SetInterruptEveryLine` | `inline void V9_SetInterruptEveryLine()` | verified |
| `V9_SetInterruptLine` | `inline void V9_SetInterruptLine(u16 line)` | verified |
| `V9_SetInterruptX` | `inline void V9_SetInterruptX(u8 x)` | verified |
| `V9_SetLayerPriority` | `inline void V9_SetLayerPriority(u8 x, u8 y)` | verified |
| `V9_SetPalette` | `inline void V9_SetPalette(u8 first, u8 num, const u8* table)` | verified |
| `V9_SetPaletteAll` | `inline void V9_SetPaletteAll(const u8* table)` | verified |
| `V9_SetPaletteEntry` | `void V9_SetPaletteEntry(u8 index, const u8* color)` | verified |
| `V9_SetPort` | `void V9_SetPort(u8 port, u8 value) __PRESERVES(b, d, e, h, iyl, iyh)` | verified |
| `V9_SetReadAddress` | `void V9_SetReadAddress(u32 addr) __PRESERVES(b, iyl, iyh)` | verified |
| `V9_SetRegister` | `void V9_SetRegister(u8 reg, u8 val) __PRESERVES(b, c, d, e, h, iyl, iyh)` | verified |
| `V9_SetRegister16` | `void V9_SetRegister16(u8 reg, u16 val) __PRESERVES(b, h, l, iyl, iyh)` | verified |
| `V9_SetScreenMode` | `void V9_SetScreenMode(u8 mode)` | verified |
| `V9_SetScrolling` | `inline void V9_SetScrolling(u16 x, u16 y)` | verified |
| `V9_SetScrollingB` | `inline void V9_SetScrollingB(u16 x, u16 y)` | verified |
| `V9_SetScrollingBX` | `void V9_SetScrollingBX(u16 x)` | verified |
| `V9_SetScrollingBY` | `void V9_SetScrollingBY(u16 y)` | verified |
| `V9_SetScrollingX` | `void V9_SetScrollingX(u16 x)` | verified |
| `V9_SetScrollingY` | `void V9_SetScrollingY(u16 y)` | verified |
| `V9_SetSpriteDisableP1` | `inline void V9_SetSpriteDisableP1(u8 id, bool disable)` | verified |
| `V9_SetSpriteDisableP2` | `inline void V9_SetSpriteDisableP2(u8 id, bool disable)` | verified |
| `V9_SetSpriteEnable` | `inline void V9_SetSpriteEnable(bool enable)` | verified |
| `V9_SetSpriteEnableP1` | `inline void V9_SetSpriteEnableP1(u8 id, bool enable)` | verified |
| `V9_SetSpriteEnableP2` | `inline void V9_SetSpriteEnableP2(u8 id, bool enable)` | verified |
| `V9_SetSpriteInfoP1` | `inline void V9_SetSpriteInfoP1(u8 id, u8 info)` | verified |
| `V9_SetSpriteInfoP2` | `inline void V9_SetSpriteInfoP2(u8 id, u8 info)` | verified |
| `V9_SetSpriteP1` | `inline void V9_SetSpriteP1(u8 id, const V9_Sprite* attr)` | verified |
| `V9_SetSpriteP2` | `inline void V9_SetSpriteP2(u8 id, const V9_Sprite* attr)` | verified |
| `V9_SetSpritePaletteOffset` | `inline void V9_SetSpritePaletteOffset(u8 offset)` | verified |
| `V9_SetSpritePaletteP1` | `inline void V9_SetSpritePaletteP1(u8 id, u8 pal)` | verified |
| `V9_SetSpritePaletteP2` | `inline void V9_SetSpritePaletteP2(u8 id, u8 pal)` | verified |
| `V9_SetSpritePatternAddr` | `inline void V9_SetSpritePatternAddr(u8 addr)` | verified |
| `V9_SetSpritePatternP1` | `inline void V9_SetSpritePatternP1(u8 id, u8 pat)` | verified |
| `V9_SetSpritePatternP2` | `inline void V9_SetSpritePatternP2(u8 id, u8 pat)` | verified |
| `V9_SetSpritePositionP1` | `inline void V9_SetSpritePositionP1(u8 id, u8 x, u8 y)` | verified |
| `V9_SetSpritePositionP2` | `inline void V9_SetSpritePositionP2(u8 id, u8 x, u8 y)` | verified |
| `V9_SetSpritePriorityP1` | `inline void V9_SetSpritePriorityP1(u8 id, u8 prio)` | verified |
| `V9_SetSpritePriorityP2` | `inline void V9_SetSpritePriorityP2(u8 id, u8 prio)` | verified |
| `V9_SetVBlankInterrupt` | `inline void V9_SetVBlankInterrupt(bool enable)` | verified_divergent |
| `V9_SetWriteAddress` | `void V9_SetWriteAddress(u32 addr) __PRESERVES(b, iyl, iyh)` | verified |
| `V9_TileAddrP1A` | `inline u32 V9_TileAddrP1A(u8 x, u8 y)` | verified |
| `V9_TileAddrP1B` | `inline u32 V9_TileAddrP1B(u8 x, u8 y)` | verified |
| `V9_TileAddrP2` | `inline u32 V9_TileAddrP2(u8 x, u8 y)` | verified |
| `V9_WaitCmdEnd` | `inline void V9_WaitCmdEnd()` | verified |
| `V9_WriteVRAM` | `inline void V9_WriteVRAM(u32 addr, const u8* src, u16 count)` | verified |
| `V9_WriteVRAM_CurrentAddr` | `void V9_WriteVRAM_CurrentAddr(const u8* src, u16 count)` | verified |

## vdp (119 functions)

| function | signature | status |
|---|---|---|
| `VDP_CleanBlinkScreen` | `inline void VDP_CleanBlinkScreen()` | verified |
| `VDP_ClearVRAM` | `void VDP_ClearVRAM()` | verified |
| `VDP_CommandCustomR32` | `void VDP_CommandCustomR32(const VDP_Command* data)` | verified |
| `VDP_CommandCustomR36` | `void VDP_CommandCustomR36(const VDP_Command36* data)` | verified |
| `VDP_CommandReadLoop` | `VDP_CommandReadLoop(u8* addr)` | verified_divergent |
| `VDP_CommandSetupR32` | `void VDP_CommandSetupR32()` | verified |
| `VDP_CommandSetupR36` | `void VDP_CommandSetupR36()` | verified |
| `VDP_CommandWait` | `void VDP_CommandWait() __PRESERVES(b, c, d, e, h, l, iyl, iyh)` | verified |
| `VDP_CommandWriteLoop` | `void VDP_CommandWriteLoop(const u8* addr) __FASTCALL __PRESERVES(d, e, iyl, iyh)` | verified |
| `VDP_DisableSprite` | `inline void VDP_DisableSprite()` | verified |
| `VDP_DisableSpritesFrom` | `void VDP_DisableSpritesFrom(u8 index)` | verified |
| `VDP_EnableDisplay` | `inline void VDP_EnableDisplay(bool enable)` | verified |
| `VDP_EnableHBlank` | `inline void VDP_EnableHBlank(bool enable)` | verified |
| `VDP_EnableMask` | `inline void VDP_EnableMask(u8 enable)` | verified |
| `VDP_EnableSprite` | `inline void VDP_EnableSprite(u8 enable)` | verified |
| `VDP_EnableTransparency` | `inline void VDP_EnableTransparency(u8 enable)` | verified |
| `VDP_EnableVBlank` | `inline void VDP_EnableVBlank(bool enable)` | verified |
| `VDP_ExpendCommand` | `inline void VDP_ExpendCommand(u8 enable)` | verified |
| `VDP_FastFillVRAM_16K` | `void VDP_FastFillVRAM_16K(u8 value, u16 dest, u16 count)` | verified_divergent |
| `VDP_FillLayout_GM2` | `void VDP_FillLayout_GM2(u8 value, u8 dx, u8 dy, u8 nx, u8 ny)` | verified |
| `VDP_FillScreen_GM2` | `inline void VDP_FillScreen_GM2(u8 value)` | verified |
| `VDP_FillVRAM_128K` | `void VDP_FillVRAM_128K(u8 value, u16 destLow, u8 destHigh, u16 count)` | verified |
| `VDP_FillVRAM_16K` | `void VDP_FillVRAM_16K(u8 value, u16 dest, u16 count)` | verified |
| `VDP_GetColorTable` | `inline VADDR VDP_GetColorTable()` | verified |
| `VDP_GetColorTable_GM2` | `inline VADDR VDP_GetColorTable_GM2(u8 bank)` | verified |
| `VDP_GetFrequency` | `inline u8 VDP_GetFrequency()` | verified |
| `VDP_GetLayoutTable` | `inline VADDR VDP_GetLayoutTable()` | verified |
| `VDP_GetMode` | `inline u8 VDP_GetMode()` | verified |
| `VDP_GetPatternTable` | `inline VADDR VDP_GetPatternTable()` | verified |
| `VDP_GetPatternTable_GM2` | `inline VADDR VDP_GetPatternTable_GM2(u8 bank)` | verified |
| `VDP_GetSpriteAttributeTable` | `inline VADDR VDP_GetSpriteAttributeTable()` | verified |
| `VDP_GetSpriteColorTable` | `inline VADDR VDP_GetSpriteColorTable()` | verified |
| `VDP_GetSpritePatternTable` | `inline VADDR VDP_GetSpritePatternTable()` | verified |
| `VDP_GetVersion` | `VDP_GetVersion()` | verified |
| `VDP_HideAllSprites` | `inline void VDP_HideAllSprites()` | verified |
| `VDP_HideSprite` | `inline void VDP_HideSprite(u8 index)` | verified |
| `VDP_Initialize` | `void VDP_Initialize()` | verified |
| `VDP_IsBitmapMode` | `inline bool VDP_IsBitmapMode(u8 mode)` | verified |
| `VDP_IsPatternMode` | `inline bool VDP_IsPatternMode(u8 mode)` | verified |
| `VDP_LoadBankColor_GM2` | `inline void VDP_LoadBankColor_GM2(const u8* src, u8 count, u8 bank, u8 offset)` | verified |
| `VDP_LoadBankPattern_GM2` | `inline void VDP_LoadBankPattern_GM2(const u8* src, u8 count, u8 bank, u8 offset)` | verified |
| `VDP_LoadColor_GM2` | `void VDP_LoadColor_GM2(const u8* src, u8 count, u8 offset)` | verified |
| `VDP_LoadPattern_GM2` | `void VDP_LoadPattern_GM2(const u8* src, u8 count, u8 offset)` | verified |
| `VDP_LoadSpritePattern` | `void VDP_LoadSpritePattern(const u8* addr, u8 index, u8 count)` | verified |
| `VDP_Peek_128K` | `u8 VDP_Peek_128K(u16 srcLow, u8 srcHigh)` | verified |
| `VDP_Peek_16K` | `u8 VDP_Peek_16K(u16 src) __PRESERVES(b, c, d, e, iyl, iyh)` | verified |
| `VDP_Peek_GM2` | `inline u8 VDP_Peek_GM2(u8 x, u8 y)` | verified |
| `VDP_Poke_128K` | `void VDP_Poke_128K(u8 val, u16 destLow, u8 destHigh)` | verified |
| `VDP_Poke_16K` | `void VDP_Poke_16K(u8 val, u16 dest) __PRESERVES(c, h, l, iyl, iyh)` | verified |
| `VDP_Poke_GM2` | `inline void VDP_Poke_GM2(u8 x, u8 y, u8 value)` | verified |
| `VDP_ReadDefaultStatus` | `u8 VDP_ReadDefaultStatus() __PRESERVES(b, c, d, e, h, l, iyl, iyh)` | verified |
| `VDP_ReadStatus` | `u8 VDP_ReadStatus(u8 stat) __PRESERVES(b, c, d, e, h, iyl, iyh)` | verified |
| `VDP_ReadVRAM_128K` | `void VDP_ReadVRAM_128K(u16 srcLow, u8 srcHigh, u8* dest, u16 count)` | verified |
| `VDP_ReadVRAM_16K` | `void VDP_ReadVRAM_16K(u16 src, u8* dest, u16 count)` | verified |
| `VDP_RegWrite` | `void VDP_RegWrite(u8 reg, u8 value) __PRESERVES(b, c, d, e, iyl, iyh)` | verified |
| `VDP_RegWriteBak` | `void VDP_RegWriteBak(u8 reg, u8 value) __PRESERVES(d, e, iyl, iyh)` | verified |
| `VDP_RegWriteBakMask` | `void VDP_RegWriteBakMask(u8 idx, u8 mask, u8 value)` | verified |
| `VDP_ResetVRAMAddrMSB` | `inline void VDP_ResetVRAMAddrMSB()` | verified |
| `VDP_SetAdjustOffset` | `void VDP_SetAdjustOffset(u8 offset)` | verified |
| `VDP_SetAdjustOffsetXY` | `inline void VDP_SetAdjustOffsetXY(i8 x, i8 y)` | verified |
| `VDP_SetBackdropColor` | `inline void VDP_SetBackdropColor(u8 color)` | verified |
| `VDP_SetBlinkChunk` | `inline void VDP_SetBlinkChunk(u8 x, u8 y)` | verified |
| `VDP_SetBlinkChunkMask` | `inline void VDP_SetBlinkChunkMask(u8 x, u8 y, u8 mask)` | verified |
| `VDP_SetBlinkChunkX` | `inline void VDP_SetBlinkChunkX(u8 x, u8 y, u8 num)` | verified |
| `VDP_SetBlinkColor` | `inline void VDP_SetBlinkColor(u8 color)` | verified |
| `VDP_SetBlinkColor2` | `inline void VDP_SetBlinkColor2(u8 bg, u8 text)` | verified |
| `VDP_SetBlinkLine` | `inline void VDP_SetBlinkLine(u8 y)` | verified |
| `VDP_SetBlinkScreen` | `inline void VDP_SetBlinkScreen()` | verified |
| `VDP_SetBlinkTile` | `void VDP_SetBlinkTile(u8 x, u8 y)` | verified |
| `VDP_SetBlinkTime` | `inline void VDP_SetBlinkTime(u8 time)` | verified |
| `VDP_SetBlinkTime2` | `inline void VDP_SetBlinkTime2(u8 even, u8 odd)` | verified |
| `VDP_SetColor` | `inline void VDP_SetColor(u8 color)` | verified |
| `VDP_SetColor2` | `inline void VDP_SetColor2(u8 bg, u8 text)` | verified |
| `VDP_SetColorTable` | `void VDP_SetColorTable(VADDR addr)` | verified |
| `VDP_SetColorTableEx` | `inline void VDP_SetColorTableEx(VADDR addr, u8 r3, u8 r10)` | verified |
| `VDP_SetDefaultPalette` | `void VDP_SetDefaultPalette()` | verified |
| `VDP_SetFrameRender` | `inline void VDP_SetFrameRender(u8 mode)` | verified |
| `VDP_SetFrequency` | `inline void VDP_SetFrequency(u8 freq)` | verified |
| `VDP_SetGrayScale` | `inline void VDP_SetGrayScale(bool enable)` | verified |
| `VDP_SetHBlankLine` | `inline void VDP_SetHBlankLine(u8 line)` | verified |
| `VDP_SetHorizontalMode` | `inline void VDP_SetHorizontalMode(u8 mode)` | verified |
| `VDP_SetHorizontalOffset` | `void VDP_SetHorizontalOffset(u16 offset)` | verified |
| `VDP_SetInfiniteBlink` | `inline void VDP_SetInfiniteBlink()` | verified |
| `VDP_SetInterlace` | `inline void VDP_SetInterlace(bool enable)` | verified |
| `VDP_SetLayoutTable` | `void VDP_SetLayoutTable(VADDR addr)` | verified |
| `VDP_SetLayoutTableEx` | `inline void VDP_SetLayoutTableEx(VADDR addr, u8 r2)` | verified |
| `VDP_SetLineCount` | `inline void VDP_SetLineCount(u8 lines)` | verified |
| `VDP_SetMSX1Palette` | `void VDP_SetMSX1Palette()` | verified |
| `VDP_SetMode` | `void VDP_SetMode(u8 mode)` | verified |
| `VDP_SetModeFlag` | `void VDP_SetModeFlag(u8 flag)` | verified |
| `VDP_SetPage` | `void VDP_SetPage(u8 page)` | verified |
| `VDP_SetPageAlternance` | `inline void VDP_SetPageAlternance(bool enable)` | verified |
| `VDP_SetPalette` | `void VDP_SetPalette(const u8* pal) __FASTCALL __PRESERVES(d, e, iyl, iyh)` | verified |
| `VDP_SetPaletteEntry` | `void VDP_SetPaletteEntry(u8 index, u16 color)` | verified |
| `VDP_SetPatternTable` | `void VDP_SetPatternTable(VADDR addr)` | verified |
| `VDP_SetPatternTableEx` | `inline void VDP_SetPatternTableEx(VADDR addr, u8 r4)` | verified |
| `VDP_SetSprite` | `void VDP_SetSprite(u8 index, u8 x, u8 y, u8 shape)` | verified |
| `VDP_SetSpriteAttributeTable` | `void VDP_SetSpriteAttributeTable(VADDR addr)` | verified |
| `VDP_SetSpriteAttributeTableEx` | `inline void VDP_SetSpriteAttributeTableEx(VADDR addr, u8 r5, u8 r11)` | verified |
| `VDP_SetSpriteColorSM1` | `void VDP_SetSpriteColorSM1(u8 index, u8 color)` | verified |
| `VDP_SetSpriteData` | `void VDP_SetSpriteData(u8 index, const u8* data)` | verified |
| `VDP_SetSpriteExMultiColor` | `void VDP_SetSpriteExMultiColor(u8 index, u8 x, u8 y, u8 shape, const u8* ram)` | verified |
| `VDP_SetSpriteExUniColor` | `void VDP_SetSpriteExUniColor(u8 index, u8 x, u8 y, u8 shape, u8 color)` | verified |
| `VDP_SetSpriteFlag` | `inline void VDP_SetSpriteFlag(u8 flag)` | verified |
| `VDP_SetSpriteMultiColor` | `void VDP_SetSpriteMultiColor(u8 index, const u8* ram)` | verified |
| `VDP_SetSpritePattern` | `void VDP_SetSpritePattern(u8 index, u8 shape)` | verified |
| `VDP_SetSpritePatternTable` | `void VDP_SetSpritePatternTable(VADDR addr)` | verified |
| `VDP_SetSpritePatternTableEx` | `inline void VDP_SetSpritePatternTableEx(VADDR addr, u8 r6)` | verified |
| `VDP_SetSpritePosition` | `void VDP_SetSpritePosition(u8 index, u8 x, u8 y)` | verified |
| `VDP_SetSpritePositionX` | `void VDP_SetSpritePositionX(u8 index, u8 x)` | verified |
| `VDP_SetSpritePositionY` | `void VDP_SetSpritePositionY(u8 index, u8 y)` | verified |
| `VDP_SetSpriteSM1` | `void VDP_SetSpriteSM1(u8 index, u8 x, u8 y, u8 shape, u8 color)` | verified |
| `VDP_SetSpriteTables` | `inline void VDP_SetSpriteTables(VADDR patAddr, VADDR attAddr)` | verified |
| `VDP_SetSpriteUniColor` | `void VDP_SetSpriteUniColor(u8 index, u8 color)` | verified |
| `VDP_SetVerticalOffset` | `inline void VDP_SetVerticalOffset(u8 offset)` | verified |
| `VDP_SetYJK` | `inline void VDP_SetYJK(u8 mode)` | verified |
| `VDP_WriteLayout_GM2` | `void VDP_WriteLayout_GM2(const u8* src, u8 dx, u8 dy, u8 nx, u8 ny)` | verified |
| `VDP_WriteVRAM_128K` | `void VDP_WriteVRAM_128K(const u8* src, u16 destLow, u8 destHigh, u16 count)` | verified |
| `VDP_WriteVRAM_16K` | `void VDP_WriteVRAM_16K(const u8* src, u16 dest, u16 count)` | verified |

## vdp_inl (19 functions)

| function | signature | status |
|---|---|---|
| `VDP_CommandHMMC` | `void VDP_CommandHMMC(const u8* addr, u16 dx, u16 dy, u16 nx, u16 ny)` | verified |
| `VDP_CommandHMMC_Arg` | `void VDP_CommandHMMC_Arg(const u8* addr, u16 dx, u16 dy, u16 nx, u16 ny, u8 arg)` | verified |
| `VDP_CommandHMMM` | `void VDP_CommandHMMM(u16 sx, u16 sy, u16 dx, u16 dy, u16 nx, u16 ny)` | verified |
| `VDP_CommandHMMM_Arg` | `void VDP_CommandHMMM_Arg(u16 sx, u16 sy, u16 dx, u16 dy, u16 nx, u16 ny, u8 arg)` | verified |
| `VDP_CommandHMMV` | `void VDP_CommandHMMV(u16 dx, u16 dy, u16 nx, u16 ny, u8 col)` | verified |
| `VDP_CommandHMMV_Arg` | `void VDP_CommandHMMV_Arg(u16 dx, u16 dy, u16 nx, u16 ny, u8 col, u8 arg)` | verified |
| `VDP_CommandLINE` | `void VDP_CommandLINE(u16 dx, u16 dy, u16 nx, u16 ny, u8 col, u8 arg, u8 op)` | verified |
| `VDP_CommandLMCM` | `void VDP_CommandLMCM()` | verified |
| `VDP_CommandLMMC` | `void VDP_CommandLMMC(const u8* addr, u16 dx, u16 dy, u16 nx, u16 ny, u8 op)` | verified |
| `VDP_CommandLMMC_Arg` | `void VDP_CommandLMMC_Arg(const u8* addr, u16 dx, u16 dy, u16 nx, u16 ny, u8 op, u8 arg)` | verified |
| `VDP_CommandLMMM` | `void VDP_CommandLMMM(u16 sx, u16 sy, u16 dx, u16 dy, u16 nx, u16 ny, u8 op)` | verified |
| `VDP_CommandLMMM_Arg` | `void VDP_CommandLMMM_Arg(u16 sx, u16 sy, u16 dx, u16 dy, u16 nx, u16 ny, u8 op, u8 arg)` | verified |
| `VDP_CommandLMMV` | `void VDP_CommandLMMV(u16 dx, u16 dy, u16 nx, u16 ny, u8 col, u8 op)` | verified |
| `VDP_CommandLMMV_Arg` | `void VDP_CommandLMMV_Arg(u16 dx, u16 dy, u16 nx, u16 ny, u8 col, u8 op, u8 arg)` | verified |
| `VDP_CommandPOINT` | `u8 VDP_CommandPOINT(u16 sx, u16 sy)` | verified |
| `VDP_CommandPSET` | `void VDP_CommandPSET(u16 dx, u16 dy, u8 col, u8 op)` | verified |
| `VDP_CommandSRCH` | `void VDP_CommandSRCH(u16 sx, u16 sy, u8 col, u8 arg)` | verified |
| `VDP_CommandSTOP` | `void VDP_CommandSTOP()` | verified |
| `VDP_CommandYMMM` | `void VDP_CommandYMMM(u16 sy, u16 dx, u16 dy, u16 ny, u8 dir)` | verified |
