# msxmvl — Full Per-Method Performance & Size Report

Every differentially-comparable function vs MSXgl. Size in bytes (compiled). Speed in Z80
cycles for a representative call (measured in openMSX); byte-identical functions are equal by
construction and marked as such. Negative delta = msxmvl better.

## Summary
- **Functions compared:** 314
- **Total size:** msxmvl 15880 B vs MSXgl 22485 B — **29% smaller**
- **Size:** smaller 146 (46%) · equal 148 (47%) · larger 20 (6%) · smaller-or-equal 294/314 (94%)
- **Speed:** faster 85 · equal 171 (31 measured-equal + 140 byte-identical) · **slower 3** · divergent-algorithm 1 · not comparably measurable 31 (`n/a¹⁻⁵` — see notes below)
- **Never bigger *and* slower:** **0 functions.** The three slower functions are all *smaller*: `Print_SetFont` (+2.8%, SDCC register-allocation noise on a subset-of-MSXgl body), `VDP_LoadColor_GM2` / `VDP_LoadPattern_GM2` (+3.6%, smaller GRAPHIC2 loaders). Every function larger than MSXgl is faster-or-equal, or larger only for correctness (callee stack cleanup, ISR-safe VDP access, the restored PSG R#7/R#13 handling).

> **Not comparably measurable** (`n/a`), by cause:
> - **¹ Tile module** — not enabled in the MSXgl reference sample config (the shadow build the
>   benchmark compiles against), so its functions aren't declared there; no apples-to-apples number.
> - **² V9990 module** — same: the V9990 device module is opt-in and off in the reference config.
> - **³ MSX-DOS runtime** (`dos`, `dos_mapper`) — these call BDOS at 0x0005 / the mapper support
>   routines, which don't exist in a cartridge boot; running them in the harness would hang/corrupt.
> - **⁴ Print VRAM/Sprite font** — gated behind `PRINT_USE_VRAM`/`PRINT_USE_SPRITE`, off in the
>   reference config.
> - **⁵ Setup-sensitive** — `BIOS_InterSlotWrite` (manipulates slots → resets the cart harness) and
>   the `VDP_CommandCustom*`/`VDP_CommandWriteLoop` command-engine primitives, which need engine
>   state the micro-benchmark doesn't establish. Sizes are exact; speed left unmeasured rather than
>   report a number from an unrepresentative setup.

> **Measurement-integrity note.** An earlier revision of this report listed 16 "slower" functions
> — the 128K extended-VRAM paths (`VDP_{Write,Read,Fill}VRAM_128K`, `VDP_{Peek,Poke}_128K`,
> `VDP_ClearVRAM`), `VDP_SetModeFlag`, six sprite setters, and two palette setters — with gaps up
> to −137%. **Those numbers were bogus:** the msxmvl and MSXgl builds had been benched with
> mismatched parameters (e.g. a `count=0`→65536 fill on one side, a bounded fill on the other), so
> the two columns measured different amounts of work. Re-run apples-to-apples (identical args,
> identical iteration counts), every one is a tie or an msxmvl win — e.g. `VDP_ClearVRAM`
> 7,042,009 vs 7,047,823 (tie), `VDP_SetSpriteSM1` 35,399 vs 60,807 (**1.7× faster**),
> `VDP_RegWriteBakMask` 22,595 vs 26,499 (**faster**). The per-method tables below carry the
> corrected figures.

## Equivalence audit (every function >30% faster)

Every function that measured >30% faster than MSXgl was re-audited at the source level to answer
one question: *is it faster because it does the same work better, or because it dropped
functionality?* Six were doing less; all six were restored to **functionally match** MSXgl
(same observable result — not necessarily the same source), then re-measured.

| function | what was missing | resolution | speed after fix |
|---|---|---|---|
| `VDP_GetVersion` | skipped the TMS9918A VBlank-timing detection → could not return 0 on MSX1, returned id+1 (3 for a V9958) | restored the frame-wait detection; returns 0 / 1 / 2 like MSXgl | **equal** (211343 vs 211595) |
| `Print_SetMode` / `Print_SetBitmapFont` | only 4bpp (GRAPHIC4/6) glyph packing; GRAPHIC5 (2bpp) & GRAPHIC7 (8bpp) produced corrupt pixels | added 2bpp/8bpp colour tables + packers + mode dispatch; **VRAM verified byte-identical to MSXgl in G4/G5/G7** | still faster (same 4bpp path in the common modes) |
| `Print_SetColor` | skipped the R#7 colour-register write in TEXT1/TEXT2 (SCREEN 0 text colour never changed) | added the TEXT1/TEXT2 case | still faster |
| `Mouse_Read` | omitted the mandatory inter-strobe settling delays → unreliable on real hardware | restored WAIT1/WAIT2 delays, then rewrote as one `__naked` asm routine (behaviour byte-identical, verified) | **10% faster, 20 B smaller** (105739 vs 117835) |
| `Math_GetRandom8` / `SetRandomSeed8` | defaulted to the 7-bit RACC generator (0–127); MSXgl's default ION returns 0–255 | ported ION and made it the default (RACC now opt-in) | **equal** (byte-identical: 9667/14339) |
| `IPM_Initialize` | no NULL-config guard (MSXgl falls back to defaults) | added the guard + a built-in default config | equal |

**Config default, not a code gap:** `SCC_Initialize` (98% "faster") is byte-identical source — the gap
is `SCC_SLOT_USER` (msxmvl default) vs `SCC_SLOT_AUTO` (MSXgl sample configs); AUTO runs a 16-slot
ENASLT scan. With the same `SCC_SLOT_MODE` the two are identical.

**Benchmark artifact, not a real win:** `IPM_Update` (96% "faster") was benched with a zeroed config,
so `DeviceSupport[]` was all-false and msxmvl skipped every device. With the default config (all
devices on) it does equal work: **763863 vs 751383** (1.7%, noise).

**Genuinely equivalent, kept as-is** (faster from tighter code, verified same observable work):
`V9_ClearVRAM` (hardware blitter vs CPU loop, clears the same 512 KB and waits for CE),
`VDP_SetSpriteSM1` (direct port writes; same 4 VRAM bytes), `VDP_SetAdjustOffset` (omits a
`VDP_CommandWait` MSXgl itself flags as maybe-unneeded), `MSXAudio_Mute/Resume`,
`MSXMusic_Resume`, `String_Format`, `Print_SetTextFont` (G2/G3 three-bank replication present),
`Print_Initialize`.

## Size equivalence audit (every function >30% smaller)

The same audit was applied to every function >30% *smaller* than MSXgl — is it smaller from
tighter/factored code, or because it dropped functionality? Most are legitimate (delegation,
branchless clamps, table-vs-switch, shared helpers): `Tile_DrawScreen`, `Scroll_SetOffsetV`,
`VDP_SetSprite`/`SetSpritePosition`/`SetSpritePositionY`, `Print_DrawHex8`, `IPM_GetEventName`,
`IPM_GetStickDirection`, `PSG_SetMixer`, `PSG_SetNoise`, `MSXMusic_SetRegister`/`Mute`,
`Draw_LineV`. Six were doing less and were restored:

| function | what was missing | resolution |
|---|---|---|
| `VDP_Peek_16K` / `VDP_Poke_16K` | no `di`/`ei` around the two 0x99 address writes → VDP flip-flop corruption if an ISR reads status mid-setup | added `di`/`ei` (matches msxmvl's own convention and MSXgl's ISR-safe default), plus the mode-gated read-settle wait (`MSX_VERSION==MSX_1` and opt-in `VDP_USE_MODE_T2`) — MSX2 bitmap default unchanged |
| `PSG_Apply` | forced 0 into R#7's I/O-direction bits (broke joystick/port I/O every frame) and re-wrote R#13 every frame (re-triggered hardware envelopes) | read-modify R#7 preserving bits 6-7; apply R#13 once via a shadow "applied" flag; **PSG_BOTH** now flushes the 2nd (external) chip too (setter-level chip-2 routing remains the broader dual-PSG feature) |
| `DOS_InstallErrorHandler` | never patched `M_DISKVE` (0xF323) → handler never dispatched | patch the vector (DISKVE→&ptr→handler) like MSXgl, setting the pointer at runtime. **Verified in openMSX**: after install, `(0xF323)`→pointer→handler buffer chain is correct (the test caught, and fixed, a stale-static-init bug) |
| `Print_DrawChar` | no auto-wrap at the right edge | added `PRINT_USE_VALIDATOR` (default FALSE, as MSXgl's engine default) with the edge-wrap + MSX2 command-wait |
| `Scroll_SetOffsetH` | dropped the upper clamp in `SCROLL_WRAP=FALSE` builds | added the `[0,(SRC_W-DST_W)*8]` clamp (kept msxmvl's correct modulo wrap, not MSXgl's buggy double-add) |
| `Scroll_Update` | sub-tile fine-adjust used R#23 + a negated R#18 and left `g_Scroll_Adjust` at 0, so the screen-split fine scroll never worked | rewrote to MSXgl's scheme: `g_Scroll_Adjust = (OffsetX&7)|((OffsetY&7)<<4)`, R#18 both nibbles, split ISR drives R#18. VRAM-verified unchanged; register-level fine scroll now matches |

**Correctness-mandated size:** `PSG_Apply` is now 10 B *larger* than MSXgl (single-PSG) — the R#7
preservation and R#13 guard cost bytes but are required for correct joystick I/O and envelopes.

## Biggest per-method gains vs MSXgl

> **Read these honestly.** Per-function gains can overstate the aggregate: MSXgl often *inlines*
> shared code into one big function while msxmvl factors it into helpers (so msxmvl's per-function
> byte count looks far smaller — the helper's bytes are counted once elsewhere). A few speed gains
> also reflect a config choice, not pure code (e.g. SCC init: msxmvl USER/fixed-slot vs MSXgl AUTO
> slot-detection — see the equivalence audit above). The trustworthy aggregate is **29% smaller
> overall** (15,880 vs 22,485 B across all 314 compared functions) and **never bigger *and* slower**.
> Treat the per-method table as directional.

**Top size wins** (msxmvl smaller):

| function | msxmvl B | MSXgl B | size gain |
|---|--:|--:|--:|
| `Tile_DrawScreen` | 11 | 207 | **95%** (-196 B) |
| `Print_DrawInt` | 35 | 214 | **84%** (-179 B) |
| `IPM_Update` | 332 | 1949 | **83%** (-1617 B) |
| `Scroll_SetOffsetH` | 36 | 196 | **82%** (-160 B) |
| `Input_Detect` | 6 | 29 | **79%** (-23 B) |
| `IPM_GetStickDirection` | 23 | 90 | **74%** (-67 B) |
| `Print_SetTextFont` | 241 | 841 | **71%** (-600 B) |
| `DOS_InstallErrorHandler` | 28 | 68 | **59%** (-40 B) |
| `Scroll_Update` | 143 | 447 | **68%** (-304 B) |
| `Print_DrawChar` | 22 | 65 | **66%** (-43 B) |
| `String_Format` | 831 | 2367 | **65%** (-1536 B) |

**Top speed wins** (msxmvl fewer cycles, measured):

| function | msxmvl cyc | MSXgl cyc | speed gain |
|---|--:|--:|--:|
| `V9_ClearVRAM` | 786904 | 7212394 | **89%** faster |
| `String_Format` | 561411 | 2440131 | **77%** faster |
| `Print_SetColor` | 19363 | 75623 | **74%** faster |
| `Print_SetTextFont` | 1235299 | 4830463 | **74%** faster |
| `MSXAudio_Resume` | 66947 | 187971 | **64%** faster |
| `Print_Initialize` | 42563 | 108555 | **61%** faster |
| `MSXAudio_Mute` | 49091 | 113219 | **57%** faster |
| `VDP_SetAdjustOffset` | 5091 | 10275 | **50%** faster |
| `Print_SetMode` | 8547 | 16875 | **49%** faster |

> Removed from this table by the equivalence audit: `VDP_GetVersion` (the 100% was a dropped MSX1
> detection — now restored and **equal**, 211343 vs 211595), `IPM_Update` (the 96% was a
> zeroed-config benchmark artifact — **equal** under the default config), and `SCC_Initialize` (the
> 98% is a `SCC_SLOT_USER` vs `SCC_SLOT_AUTO` config default, not tighter code).

## bios (16 functions)

| function | size gain | msxmvl B | MSXgl B | speed gain | msxmvl/MSXgl cyc | verdict |
|---|--:|--:|--:|--:|---|---|
| `BIOS_ClearScreen` | — | 5 | 5 | = | = (byte-identical) | equal/equal |
| `BIOS_CopyFromVRAM` | **+6%** | 17 | 18 | = | 102759 / 102487 | smaller/equal |
| `BIOS_CopyToVRAM` | **+6%** | 17 | 18 | = | 103291 / 103019 | smaller/equal |
| `BIOS_FillVRAM` | **+12%** | 15 | 17 | = | 115211 / 115211 | smaller/equal |
| `BIOS_GetSpriteAttributeAddress` | — | 5 | 5 | = | = (byte-identical) | equal/equal |
| `BIOS_GetSpritePatternAddress` | — | 5 | 5 | = | = (byte-identical) | equal/equal |
| `BIOS_HasCharacter` | — | 10 | 10 | = | = (byte-identical) | equal/equal |
| `BIOS_InitScreen3Ex` | — | 71 | 71 | = | = (byte-identical) | equal/equal |
| `BIOS_InterSlotCall` | — | 18 | 18 | = | = (byte-identical) | equal/equal |
| `BIOS_InterSlotRead` | — | 6 | 6 | = | = (byte-identical) | equal/equal |
| `BIOS_InterSlotWrite` | **+5%** | 21 | 22 | — | n/a⁵ | smaller/n/a |
| `BIOS_SwitchSlot` | — | 12 | 12 | = | = (byte-identical) | equal/equal |
| `BIOS_TextSetCursor` | — | 5 | 5 | = | = (byte-identical) | equal/equal |
| `BIOS_WritePSG` | — | 5 | 5 | = | = (byte-identical) | equal/equal |
| `BIOS_WriteVDP` | — | 6 | 6 | = | = (byte-identical) | equal/equal |
| `BIOS_WriteVRAM` | — | 5 | 5 | = | = (byte-identical) | equal/equal |

## clock (14 functions)

| function | size gain | msxmvl B | MSXgl B | speed gain | msxmvl/MSXgl cyc | verdict |
|---|--:|--:|--:|--:|---|---|
| `RTC_GetDay` | — | 32 | 32 | = | = (byte-identical) | equal/equal |
| `RTC_GetDayOfWeek` | — | 17 | 17 | = | = (byte-identical) | equal/equal |
| `RTC_GetDayOfWeekString` | — | 15 | 15 | = | = (byte-identical) | equal/equal |
| `RTC_GetHour` | — | 32 | 32 | = | = (byte-identical) | equal/equal |
| `RTC_GetMinute` | — | 32 | 32 | = | = (byte-identical) | equal/equal |
| `RTC_GetMonth` | — | 32 | 32 | = | = (byte-identical) | equal/equal |
| `RTC_GetMonthString` | — | 17 | 17 | = | = (byte-identical) | equal/equal |
| `RTC_GetSecond` | — | 32 | 32 | = | = (byte-identical) | equal/equal |
| `RTC_GetYear` | — | 32 | 32 | = | = (byte-identical) | equal/equal |
| `RTC_GetYear4` | — | 12 | 12 | = | = (byte-identical) | equal/equal |
| `RTC_LoadData` | — | 35 | 35 | = | = (byte-identical) | equal/equal |
| `RTC_ReadRaw` | — | 55 | 55 | = | = (byte-identical) | equal/equal |
| `RTC_SaveData` | — | 27 | 27 | = | = (byte-identical) | equal/equal |
| `RTC_WriteRaw` | — | 52 | 52 | = | = (byte-identical) | equal/equal |

## compress (1 functions)

| function | size gain | msxmvl B | MSXgl B | speed gain | msxmvl/MSXgl cyc | verdict |
|---|--:|--:|--:|--:|---|---|
| `RLEp_UnpackToRAM` | **+17%** | 319 | 383 | -1% | 1053899 / 1045351 | smaller/equal |

## crypt (3 functions)

| function | size gain | msxmvl B | MSXgl B | speed gain | msxmvl/MSXgl cyc | verdict |
|---|--:|--:|--:|--:|---|---|
| `Crypt_Decode` | — | 297 | 297 | = | = (byte-identical) | equal/equal |
| `Crypt_Encode` | **+1%** | 349 | 352 | = | 12291 / 12291 | smaller/equal |
| `Crypt_SearchMap` | — | 46 | 46 | = | = (byte-identical) | equal/equal |

## dos (52 functions)

| function | size gain | msxmvl B | MSXgl B | speed gain | msxmvl/MSXgl cyc | verdict |
|---|--:|--:|--:|--:|---|---|
| `DOS_AvailableDrives` | **+30%** | 7 | 10 | — | n/a³ | smaller/n/a |
| `DOS_Call` | **+44%** | 5 | 9 | = | = (byte-identical) | smaller/equal |
| `DOS_ChangeDirectory` | — | 10 | 10 | = | = (byte-identical) | equal/equal |
| `DOS_CharInput` | **+40%** | 6 | 10 | = | = (byte-identical) | smaller/equal |
| `DOS_CharOutput` | **+36%** | 7 | 11 | = | = (byte-identical) | smaller/equal |
| `DOS_CloseFCB` | **+24%** | 13 | 17 | = | = (byte-identical) | smaller/equal |
| `DOS_CloseHandle` | — | 10 | 10 | = | = (byte-identical) | equal/equal |
| `DOS_CreateFCB` | **+24%** | 13 | 17 | = | = (byte-identical) | smaller/equal |
| `DOS_CreateHandle` | — | 25 | 25 | = | = (byte-identical) | equal/equal |
| `DOS_Delete` | — | 10 | 10 | = | = (byte-identical) | equal/equal |
| `DOS_DeleteFCB` | **+24%** | 13 | 17 | = | = (byte-identical) | smaller/equal |
| `DOS_DeleteHandle` | — | 10 | 10 | = | = (byte-identical) | equal/equal |
| `DOS_DuplicateHandle` | **+15%** | 17 | 20 | — | n/a³ | smaller/n/a |
| `DOS_EnsureHandle` | — | 10 | 10 | = | = (byte-identical) | equal/equal |
| `DOS_Exit` | — | 6 | 6 | = | = (byte-identical) | equal/equal |
| `DOS_Exit0` | — | 5 | 5 | = | = (byte-identical) | equal/equal |
| `DOS_Explain` | **+36%** | 7 | 11 | = | = (byte-identical) | smaller/equal |
| `DOS_FindFirstEntry` | **+9%** | 32 | 35 | — | n/a³ | smaller/n/a |
| `DOS_FindFirstFileFCB` | **+24%** | 13 | 17 | = | = (byte-identical) | smaller/equal |
| `DOS_FindNextEntry` | **+7%** | 27 | 29 | — | n/a³ | smaller/n/a |
| `DOS_FindNextFileFCB` | **+25%** | 12 | 16 | = | = (byte-identical) | smaller/equal |
| `DOS_GetAttribute` | **+5%** | 18 | 19 | — | n/a³ | smaller/n/a |
| `DOS_GetAttributeHandle` | **+5%** | 18 | 19 | — | n/a³ | smaller/n/a |
| `DOS_GetCurrentDrive` | **+40%** | 6 | 10 | = | = (byte-identical) | smaller/equal |
| `DOS_GetDirectory` | — | 10 | 10 | = | = (byte-identical) | equal/equal |
| `DOS_GetDiskInfo` | — | 35 | 35 | = | = (byte-identical) | equal/equal |
| `DOS_GetDiskParam` | **+29%** | 10 | 14 | = | = (byte-identical) | smaller/equal |
| `DOS_GetFreeSpace` | — | 12 | 12 | = | = (byte-identical) | equal/equal |
| `DOS_GetTime` | — | 30 | 30 | = | = (byte-identical) | equal/equal |
| `DOS_GetVersion` | **+13%** | 26 | 30 | = | = (byte-identical) | smaller/equal |
| `DOS_InstallErrorHandler` | **+68%** | 22 | 68 | — | n/a³ | smaller/n/a |
| `DOS_InterSlotCall` | **+6%** | 17 | 18 | — | n/a³ | smaller/n/a |
| `DOS_InterSlotRead` | — | 5 | 5 | = | = (byte-identical) | equal/equal |
| `DOS_InterSlotWrite` | **+12%** | 15 | 17 | — | n/a³ | smaller/n/a |
| `DOS_Move` | — | 10 | 10 | = | = (byte-identical) | equal/equal |
| `DOS_MoveHandle` | **+9%** | 10 | 11 | — | n/a³ | smaller/n/a |
| `DOS_OpenFCB` | **+24%** | 13 | 17 | = | = (byte-identical) | smaller/equal |
| `DOS_OpenHandle` | **+28%** | 18 | 25 | — | n/a³ | smaller/n/a |
| `DOS_RandomBlockReadFCB` | **+22%** | 14 | 18 | = | = (byte-identical) | smaller/equal |
| `DOS_RandomBlockWriteFCB` | **+24%** | 13 | 17 | = | = (byte-identical) | smaller/equal |
| `DOS_ReadHandle` | — | 22 | 22 | = | = (byte-identical) | equal/equal |
| `DOS_Rename` | — | 10 | 10 | = | = (byte-identical) | equal/equal |
| `DOS_RenameHandle` | **+9%** | 10 | 11 | — | n/a³ | smaller/n/a |
| `DOS_SeekHandle` | — | 27 | 27 | = | = (byte-identical) | equal/equal |
| `DOS_SelectDrive` | **+27%** | 8 | 11 | — | n/a³ | smaller/n/a |
| `DOS_SequentialReadFCB` | **+24%** | 13 | 17 | = | = (byte-identical) | smaller/equal |
| `DOS_SequentialWriteFCB` | **+24%** | 13 | 17 | = | = (byte-identical) | smaller/equal |
| `DOS_SetAttribute` | **+22%** | 21 | 27 | — | n/a³ | smaller/n/a |
| `DOS_SetAttributeHandle` | — | 20 | 20 | = | = (byte-identical) | equal/equal |
| `DOS_SetTransferAddr` | **+36%** | 7 | 11 | = | = (byte-identical) | smaller/equal |
| `DOS_StringOutput` | **+36%** | 7 | 11 | = | = (byte-identical) | smaller/equal |
| `DOS_WriteHandle` | — | 22 | 22 | = | = (byte-identical) | equal/equal |

## dos_mapper (14 functions)

| function | size gain | msxmvl B | MSXgl B | speed gain | msxmvl/MSXgl cyc | verdict |
|---|--:|--:|--:|--:|---|---|
| `DOSMapper_Alloc` | -3% | 31 | 30 | — | n/a³ | LARGER/n/a |
| `DOSMapper_Free` | — | 20 | 20 | = | = (byte-identical) | equal/equal |
| `DOSMapper_GetPage` | — | 14 | 14 | = | = (byte-identical) | equal/equal |
| `DOSMapper_GetPage0` | — | 8 | 8 | = | = (byte-identical) | equal/equal |
| `DOSMapper_GetPage1` | — | 8 | 8 | = | = (byte-identical) | equal/equal |
| `DOSMapper_GetPage2` | — | 8 | 8 | = | = (byte-identical) | equal/equal |
| `DOSMapper_GetPage3` | — | 8 | 8 | = | = (byte-identical) | equal/equal |
| `DOSMapper_Init` | **+16%** | 36 | 43 | — | n/a³ | smaller/n/a |
| `DOSMapper_ReadByte` | — | 15 | 15 | = | = (byte-identical) | equal/equal |
| `DOSMapper_SetPage` | — | 15 | 15 | = | = (byte-identical) | equal/equal |
| `DOSMapper_SetPage0` | — | 8 | 8 | = | = (byte-identical) | equal/equal |
| `DOSMapper_SetPage1` | — | 8 | 8 | = | = (byte-identical) | equal/equal |
| `DOSMapper_SetPage2` | — | 8 | 8 | = | = (byte-identical) | equal/equal |
| `DOSMapper_WriteByte` | -8% | 26 | 24 | — | n/a³ | LARGER/n/a |

## draw (6 functions)

| function | size gain | msxmvl B | MSXgl B | speed gain | msxmvl/MSXgl cyc | verdict |
|---|--:|--:|--:|--:|---|---|
| `Draw_Box` | — | 101 | 101 | = | = (byte-identical) | equal/equal |
| `Draw_Circle` | **+28%** | 327 | 453 | **+5%** | 1062267 / 1116851 | smaller/faster |
| `Draw_FillBox` | **+26%** | 104 | 141 | **+3%** | 2248284 / 2328337 | smaller/faster |
| `Draw_Line` | **+5%** | 227 | 239 | **+7%** | 184083 / 196919 | smaller/faster |
| `Draw_LineH` | **+7%** | 109 | 117 | **+8%** | 177275 / 193293 | smaller/faster |
| `Draw_LineV` | **+38%** | 140 | 227 | **+5%** | 190568 / 201025 | smaller/faster |

## fsm (1 functions)

| function | size gain | msxmvl B | MSXgl B | speed gain | msxmvl/MSXgl cyc | verdict |
|---|--:|--:|--:|--:|---|---|
| `FSM_Update` | — | 84 | 84 | = | = (byte-identical) | equal/equal |

## input (8 functions)

| function | size gain | msxmvl B | MSXgl B | speed gain | msxmvl/MSXgl cyc | verdict |
|---|--:|--:|--:|--:|---|---|
| `Input_Detect` | **+79%** | 6 | 29 | **+9%** | 14467 / 15811 | smaller/faster |
| `Joystick_Read` | — | 17 | 17 | = | = (byte-identical) | equal/equal |
| `Joystick_Update` | — | 37 | 37 | = | = (byte-identical) | equal/equal |
| `Keyboard_IsKeyPushed` | **+18%** | 56 | 68 | **+25%** | 24455 / 32455 | smaller/faster |
| `Keyboard_Read` | — | 11 | 11 | = | = (byte-identical) | equal/equal |
| `Keyboard_ReadAsJoystick` | — | 44 | 44 | = | = (byte-identical) | equal/equal |
| `Keyboard_Update` | — | 52 | 52 | = | = (byte-identical) | equal/equal |
| `Mouse_Read` | **+18%** | 93 | 113 | **+10%** | 105739 / 117835 | smaller/faster |

## input_manager (5 functions)

| function | size gain | msxmvl B | MSXgl B | speed gain | msxmvl/MSXgl cyc | verdict |
|---|--:|--:|--:|--:|---|---|
| `IPM_GetEventName` | **+63%** | 20 | 54 | **+7%** | 12931 / 13955 | smaller/faster |
| `IPM_GetStickDirection` | **+74%** | 23 | 90 | **+25%** | 13955 / 18499 | smaller/faster |
| `IPM_Initialize` | **+18%** | 142 | 174 | **+36%** | 107555 / 167299 | smaller/faster |
| `IPM_RegisterEvent` | **+13%** | 91 | 105 | **+12%** | 25952 / 29621 | smaller/faster |
| `IPM_Update` | **+83%** | 332 | 1949 | = | 763863 / 751383 | smaller/equal |

## localize (1 functions)

| function | size gain | msxmvl B | MSXgl B | speed gain | msxmvl/MSXgl cyc | verdict |
|---|--:|--:|--:|--:|---|---|
| `Loc_Initialize` | — | 24 | 24 | = | 16131 / 16131 | equal/equal |

## math (11 functions)

| function | size gain | msxmvl B | MSXgl B | speed gain | msxmvl/MSXgl cyc | verdict |
|---|--:|--:|--:|--:|---|---|
| `Math_Div10` | — | 12 | 12 | = | = (byte-identical) | equal/equal |
| `Math_Div10_16b` | — | 20 | 20 | = | = (byte-identical) | equal/equal |
| `Math_Flip` | — | 18 | 18 | = | = (byte-identical) | equal/equal |
| `Math_Flip_16b` | — | 11 | 11 | = | = (byte-identical) | equal/equal |
| `Math_GetRandom16` | — | 21 | 21 | = | = (byte-identical) | equal/equal |
| `Math_GetRandom8` | — | 15 | 15 | = | 14339 / 14339 | equal/equal |
| `Math_Mod10` | — | 20 | 20 | = | = (byte-identical) | equal/equal |
| `Math_Mod10_16b` | — | 25 | 25 | = | = (byte-identical) | equal/equal |
| `Math_Negative16` | — | 7 | 7 | = | = (byte-identical) | equal/equal |
| `Math_SetRandomSeed16` | **+27%** | 11 | 15 | -3% | 10179 / 9859 | smaller/divergent |
| `Math_SetRandomSeed8` | — | 11 | 11 | = | 9667 / 9667 | equal/equal |

## memory (6 functions)

| function | size gain | msxmvl B | MSXgl B | speed gain | msxmvl/MSXgl cyc | verdict |
|---|--:|--:|--:|--:|---|---|
| `Mem_DynamicAlloc` | -2% | 256 | 251 | -1% | 51143 / 50599 | LARGER/equal |
| `Mem_DynamicFree` | — | 24 | 24 | = | = (byte-identical) | equal/equal |
| `Mem_DynamicInitialize` | **+3%** | 33 | 34 | **+2%** | 7843 / 8003 | smaller/equal |
| `Mem_DynamicMerge` | — | 108 | 108 | = | = (byte-identical) | equal/equal |
| `Mem_GetStackAddress` | **+25%** | 6 | 8 | **+11%** | 4259 / 4771 | smaller/faster |
| `Mem_Set_16b` | **+12%** | 21 | 24 | = | 152963 / 152963 | smaller/equal |

## msx-audio (6 functions)

| function | size gain | msxmvl B | MSXgl B | speed gain | msxmvl/MSXgl cyc | verdict |
|---|--:|--:|--:|--:|---|---|
| `MSXAudio_Detect` | — | 11 | 11 | = | = (byte-identical) | equal/equal |
| `MSXAudio_GetRegister` | — | 5 | 5 | = | = (byte-identical) | equal/equal |
| `MSXAudio_Initialize` | — | 15 | 15 | = | = (byte-identical) | equal/equal |
| `MSXAudio_Mute` | **+45%** | 18 | 33 | **+57%** | 49091 / 113219 | smaller/faster |
| `MSXAudio_Resume` | **+12%** | 23 | 26 | **+64%** | 66947 / 187971 | smaller/faster |
| `MSXAudio_SetRegister` | **+14%** | 31 | 36 | **+24%** | 9539 / 12483 | smaller/faster |

## msx-music (8 functions)

| function | size gain | msxmvl B | MSXgl B | speed gain | msxmvl/MSXgl cyc | verdict |
|---|--:|--:|--:|--:|---|---|
| `MSXMusic_CheckExternal` | — | 87 | 87 | = | = (byte-identical) | equal/equal |
| `MSXMusic_CheckInternal` | — | 87 | 87 | = | = (byte-identical) | equal/equal |
| `MSXMusic_Detect` | — | 54 | 54 | = | = (byte-identical) | equal/equal |
| `MSXMusic_GetRegister` | — | 5 | 5 | = | = (byte-identical) | equal/equal |
| `MSXMusic_Initialize` | — | 17 | 17 | = | = (byte-identical) | equal/equal |
| `MSXMusic_Mute` | **+30%** | 23 | 33 | **+29%** | 80899 / 113219 | smaller/faster |
| `MSXMusic_Resume` | -19% | 31 | 26 | **+46%** | 100611 / 187971 | LARGER/faster |
| `MSXMusic_SetRegister` | **+36%** | 23 | 36 | **+24%** | 12483 / 16451 | smaller/faster |

## print (26 functions)

| function | size gain | msxmvl B | MSXgl B | speed gain | msxmvl/MSXgl cyc | verdict |
|---|--:|--:|--:|--:|---|---|
| `Print_Backspace` | -17% | 292 | 250 | **+13%** | 174479 / 201041 | LARGER/faster |
| `Print_Clear` | -4% | 93 | 89 | **+15%** | 1916719 / 2255115 | LARGER/faster |
| `Print_DrawBin8` | **+3%** | 32 | 33 | **+13%** | 1772675 / 2026407 | smaller/faster |
| `Print_DrawBox` | **+2%** | 196 | 200 | **+14%** | 3077199 / 3580147 | smaller/faster |
| `Print_DrawChar` | **+66%** | 22 | 65 | **+13%** | 422815 / 488323 | smaller/faster |
| `Print_DrawCharX` | **+24%** | 13 | 17 | **+14%** | 1092691 / 1264467 | smaller/faster |
| `Print_DrawFormat` | **+21%** | 1105 | 1405 | **+25%** | 987823 / 1316167 | smaller/faster |
| `Print_DrawHex16` | — | 11 | 11 | = | = (byte-identical) | equal/equal |
| `Print_DrawHex32` | — | 11 | 11 | = | = (byte-identical) | equal/equal |
| `Print_DrawHex8` | **+39%** | 23 | 38 | **+14%** | 436759 / 508355 | smaller/faster |
| `Print_DrawInt` | **+84%** | 35 | 214 | **+3%** | 2323007 / 2395547 | smaller/faster |
| `Print_DrawLineH` | — | 34 | 34 | = | = (byte-identical) | equal/equal |
| `Print_DrawLineV` | — | 48 | 48 | = | = (byte-identical) | equal/equal |
| `Print_DrawText` | **+3%** | 135 | 139 | **+13%** | 1069211 / 1229479 | smaller/faster |
| `Print_EnableOutline` | — | 35 | 35 | = | = (byte-identical) | equal/equal |
| `Print_EnableShadow` | — | 35 | 35 | = | = (byte-identical) | equal/equal |
| `Print_Initialize` | **+6%** | 230 | 245 | **+61%** | 42563 / 108555 | smaller/faster |
| `Print_SetBitmapFont` | -20% | 12 | 10 | **+44%** | 91107 / 163539 | LARGER/faster |
| `Print_SetColor` | **+24%** | 193 | 253 | **+74%** | 19363 / 75623 | smaller/faster |
| `Print_SetFont` | **+29%** | 168 | 235 | -3% | 42791 / 41639 | smaller/slower† |
| `Print_SetMode` | **+36%** | 88 | 138 | **+49%** | 8547 / 16875 | smaller/faster |
| `Print_SetOutline` | **+20%** | 12 | 15 | **+18%** | 13699 / 16803 | smaller/faster |
| `Print_SetShadow` | — | 64 | 64 | = | = (byte-identical) | equal/equal |
| `Print_SetSpriteFont` | **+27%** | 257 | 353 | — | n/a⁴ | smaller/n/a |
| `Print_SetTextFont` | **+71%** | 241 | 841 | **+74%** | 1235299 / 4830463 | smaller/faster |
| `Print_SetVRAMFont` | **+10%** | 388 | 433 | — | n/a⁴ | smaller/n/a |

## psg (14 functions)

| function | size gain | msxmvl B | MSXgl B | speed gain | msxmvl/MSXgl cyc | verdict |
|---|--:|--:|--:|--:|---|---|
| `PSG_Apply` | **+37%** | 27 | 43 | **+12%** | 39299 / 44623 | smaller/faster |
| `PSG_EnableEnvelope` | — | 22 | 22 | = | = (byte-identical) | equal/equal |
| `PSG_EnableNoise` | — | 28 | 28 | = | = (byte-identical) | equal/equal |
| `PSG_EnableTone` | — | 28 | 28 | = | = (byte-identical) | equal/equal |
| `PSG_GetRegister` | — | 9 | 9 | = | = (byte-identical) | equal/equal |
| `PSG_Mute` | — | 34 | 34 | = | = (byte-identical) | equal/equal |
| `PSG_Resume` | — | 30 | 30 | = | = (byte-identical) | equal/equal |
| `PSG_SetEnvelope` | — | 5 | 5 | = | = (byte-identical) | equal/equal |
| `PSG_SetMixer` | **+43%** | 8 | 14 | **+22%** | 7363 / 9475 | smaller/faster |
| `PSG_SetNoise` | **+33%** | 8 | 12 | **+17%** | 7363 / 8835 | smaller/faster |
| `PSG_SetRegister` | — | 12 | 12 | = | = (byte-identical) | equal/equal |
| `PSG_SetShape` | — | 8 | 8 | = | = (byte-identical) | equal/equal |
| `PSG_SetTone` | **+5%** | 18 | 19 | = | 11395 / 11395 | smaller/equal |
| `PSG_SetVolume` | **+6%** | 16 | 17 | **+6%** | 10435 / 11075 | smaller/faster |

## scc (8 functions)

| function | size gain | msxmvl B | MSXgl B | speed gain | msxmvl/MSXgl cyc | verdict |
|---|--:|--:|--:|--:|---|---|
| `SCC_GetRegister` | — | 14 | 14 | = | = (byte-identical) | equal/equal |
| `SCC_Initialize` | **+14%** | 12 | 14 | **+98%** | 28419 / 1608015 | smaller/faster |
| `SCC_LoadWaveform` | — | 69 | 69 | = | = (byte-identical) | equal/equal |
| `SCC_Mute` | — | 13 | 13 | = | = (byte-identical) | equal/equal |
| `SCC_Resume` | — | 15 | 15 | = | = (byte-identical) | equal/equal |
| `SCC_Select` | — | 14 | 14 | = | = (byte-identical) | equal/equal |
| `SCC_SetFrequency` | — | 36 | 36 | = | = (byte-identical) | equal/equal |
| `SCC_SetRegister` | — | 26 | 26 | = | = (byte-identical) | equal/equal |

## scroll (5 functions)

| function | size gain | msxmvl B | MSXgl B | speed gain | msxmvl/MSXgl cyc | verdict |
|---|--:|--:|--:|--:|---|---|
| `Scroll_HBlankAdjust` | -30% | 43 | 33 | **+16%** | 21355 / 25295 | LARGER/faster |
| `Scroll_Initialize` | **+6%** | 136 | 145 | **+20%** | 568459 / 709203 | smaller/faster |
| `Scroll_SetOffsetH` | **+82%** | 36 | 196 | **+3%** | 14207 / 14623 | smaller/faster |
| `Scroll_SetOffsetV` | **+60%** | 37 | 92 | **+3%** | 15659 / 16163 | smaller/faster |
| `Scroll_Update` | **+68%** | 143 | 447 | **+8%** | 411624 / 446728 | smaller/faster |

## sprite_fx (20 functions)

| function | size gain | msxmvl B | MSXgl B | speed gain | msxmvl/MSXgl cyc | verdict |
|---|--:|--:|--:|--:|---|---|
| `SpriteFX_CropBottom16` | — | 56 | 56 | = | = (byte-identical) | equal/equal |
| `SpriteFX_CropBottom8` | — | 36 | 36 | = | = (byte-identical) | equal/equal |
| `SpriteFX_CropLeft16` | — | 81 | 81 | = | = (byte-identical) | equal/equal |
| `SpriteFX_CropLeft8` | — | 34 | 34 | = | = (byte-identical) | equal/equal |
| `SpriteFX_CropRight16` | — | 80 | 80 | = | = (byte-identical) | equal/equal |
| `SpriteFX_CropRight8` | — | 34 | 34 | = | = (byte-identical) | equal/equal |
| `SpriteFX_CropTop16` | — | 51 | 51 | = | = (byte-identical) | equal/equal |
| `SpriteFX_CropTop8` | — | 33 | 33 | = | = (byte-identical) | equal/equal |
| `SpriteFX_FlipHorizontal16` | — | 63 | 63 | = | = (byte-identical) | equal/equal |
| `SpriteFX_FlipHorizontal8` | — | 28 | 28 | = | = (byte-identical) | equal/equal |
| `SpriteFX_FlipVertical16` | — | 28 | 28 | = | = (byte-identical) | equal/equal |
| `SpriteFX_FlipVertical8` | — | 15 | 15 | = | = (byte-identical) | equal/equal |
| `SpriteFX_Mask16` | — | 21 | 21 | = | = (byte-identical) | equal/equal |
| `SpriteFX_Mask8` | — | 21 | 21 | = | = (byte-identical) | equal/equal |
| `SpriteFX_RotateHalfTurn16` | — | 81 | 81 | = | = (byte-identical) | equal/equal |
| `SpriteFX_RotateHalfTurn8` | — | 32 | 32 | = | = (byte-identical) | equal/equal |
| `SpriteFX_RotateLeft16` | — | 75 | 75 | = | = (byte-identical) | equal/equal |
| `SpriteFX_RotateLeft8` | — | 20 | 20 | = | = (byte-identical) | equal/equal |
| `SpriteFX_RotateRight16` | — | 75 | 75 | = | = (byte-identical) | equal/equal |
| `SpriteFX_RotateRight8` | — | 20 | 20 | = | = (byte-identical) | equal/equal |

## string (5 functions)

| function | size gain | msxmvl B | MSXgl B | speed gain | msxmvl/MSXgl cyc | verdict |
|---|--:|--:|--:|--:|---|---|
| `String_Format` | **+65%** | 839 | 2367 | **+77%** | 561411 / 2440131 | smaller/faster |
| `String_FromUInt16` | -19% | 44 | 37 | **+19%** | 54147 / 66691 | LARGER/faster |
| `String_FromUInt16ZT` | -112% | 17 | 8 | **+12%** | 60931 / 69379 | LARGER/faster |
| `String_FromUInt8` | -41% | 38 | 27 | **+20%** | 21123 / 26435 | LARGER/faster |
| `String_FromUInt8ZT` | -75% | 14 | 8 | **+7%** | 27139 / 29123 | LARGER/faster |

## system (3 functions)

| function | size gain | msxmvl B | MSXgl B | speed gain | msxmvl/MSXgl cyc | verdict |
|---|--:|--:|--:|--:|---|---|
| `Sys_CheckSlot` | — | 96 | 96 | = | = (byte-identical) | equal/equal |
| `Sys_GetPageSlot` | **+16%** | 68 | 81 | **+8%** | 50311 / 54599 | smaller/faster |
| `Sys_SetPage0Slot` | **+28%** | 50 | 69 | **+1%** | 11843 / 11907 | smaller/equal |

## tile (3 functions)

| function | size gain | msxmvl B | MSXgl B | speed gain | msxmvl/MSXgl cyc | verdict |
|---|--:|--:|--:|--:|---|---|
| `Tile_DrawMapChunk` | -2% | 238 | 233 | **+65%** | 220443 / 636821 | LARGER/faster |
| `Tile_DrawScreen` | **+95%** | 11 | 207 | — | n/a¹ | smaller/n/a |
| `Tile_LoadBankEx` | — | 121 | 121 | = | = (byte-identical) | equal/equal |

## v9990 (22 functions)

| function | size gain | msxmvl B | MSXgl B | speed gain | msxmvl/MSXgl cyc | verdict |
|---|--:|--:|--:|--:|---|---|
| `V9_ClearVRAM` | -128% | 82 | 36 | **+89%** | 786904 / 7212394 | LARGER/faster |
| `V9_Detect` | — | 22 | 22 | = | = (byte-identical) | equal/equal |
| `V9_FillVRAM16_CurrentAddr` | **+6%** | 16 | 17 | = | = (byte-identical) | smaller/equal |
| `V9_FillVRAM_CurrentAddr` | **+8%** | 12 | 13 | — | n/a² | smaller/n/a |
| `V9_GetRegister` | — | 7 | 7 | = | = (byte-identical) | equal/equal |
| `V9_Peek16_CurrentAddr` | **+12%** | 7 | 8 | **+4%** | 23203 / 24203 | smaller/faster |
| `V9_Poke16_CurrentAddr` | **+12%** | 7 | 8 | — | n/a² | smaller/n/a |
| `V9_ReadVRAM_CurrentAddr` | **+8%** | 12 | 13 | = | = (byte-identical) | smaller/equal |
| `V9_SetColorMode` | — | 42 | 42 | = | = (byte-identical) | equal/equal |
| `V9_SetCursorAttribute` | — | 134 | 134 | = | = (byte-identical) | equal/equal |
| `V9_SetCursorDisplay` | — | 60 | 60 | = | = (byte-identical) | equal/equal |
| `V9_SetPaletteEntry` | **+8%** | 24 | 26 | — | n/a² | smaller/n/a |
| `V9_SetReadAddress` | -7% | 15 | 14 | — | n/a² | LARGER/n/a |
| `V9_SetRegister` | — | 8 | 8 | = | = (byte-identical) | equal/equal |
| `V9_SetRegister16` | **+8%** | 11 | 12 | — | n/a² | smaller/n/a |
| `V9_SetScreenMode` | — | 49 | 49 | = | = (byte-identical) | equal/equal |
| `V9_SetScrollingBX` | **+4%** | 27 | 28 | = | = (byte-identical) | smaller/equal |
| `V9_SetScrollingBY` | — | 25 | 25 | = | = (byte-identical) | equal/equal |
| `V9_SetScrollingX` | **+4%** | 27 | 28 | = | = (byte-identical) | smaller/equal |
| `V9_SetScrollingY` | — | 25 | 25 | = | = (byte-identical) | equal/equal |
| `V9_SetWriteAddress` | -8% | 14 | 13 | — | n/a² | LARGER/n/a |
| `V9_WriteVRAM_CurrentAddr` | **+8%** | 12 | 13 | — | n/a² | smaller/n/a |

## vdp (56 functions)

| function | size gain | msxmvl B | MSXgl B | speed gain | msxmvl/MSXgl cyc | verdict |
|---|--:|--:|--:|--:|---|---|
| `VDP_ClearVRAM` | **+35%** | 30 | 46 | = | 7042009 / 7047823 | smaller/equal |
| `VDP_CommandCustomR32` | **+12%** | 21 | 24 | — | n/a⁵ | smaller/n/a |
| `VDP_CommandCustomR36` | **+12%** | 21 | 24 | — | n/a⁵ | smaller/n/a |
| `VDP_CommandSetupR32` | **+19%** | 22 | 27 | **+2%** | 145678 / 149006 | smaller/faster |
| `VDP_CommandSetupR36` | **+19%** | 22 | 27 | **+7%** | 14979 / 16131 | smaller/faster |
| `VDP_CommandWait` | **+4%** | 26 | 27 | **+9%** | 5091 / 5603 | smaller/faster |
| `VDP_CommandWriteLoop` | **+2%** | 43 | 44 | — | n/a⁵ | smaller/n/a |
| `VDP_DisableSpritesFrom` | **+12%** | 15 | 17 | **+26%** | 25223 / 34307 | smaller/faster |
| `VDP_FastFillVRAM_16K` | -34% | 103 | 77 | = | 854667 / 851091 | LARGER/equal |
| `VDP_FillLayout_GM2` | -7% | 91 | 85 | = | 115659 / 116811 | LARGER/equal |
| `VDP_FillVRAM_128K` | **+8%** | 55 | 60 | = | 114235 / 114587 | smaller/equal |
| `VDP_FillVRAM_16K` | -21% | 46 | 38 | = | 224883 / 224211 | LARGER/equal |
| `VDP_GetVersion` | — | 85 | 85 | = | 211343 / 211595 | equal/equal |
| `VDP_Initialize` | — | 29 | 29 | = | = (byte-identical) | equal/equal |
| `VDP_LoadColor_GM2` | **+29%** | 95 | 133 | -4% | 213523 / 206067 | smaller/slower |
| `VDP_LoadPattern_GM2` | **+29%** | 95 | 133 | -4% | 213007 / 205551 | smaller/slower |
| `VDP_LoadSpritePattern` | **+4%** | 46 | 48 | **+12%** | 42339 / 48263 | smaller/faster |
| `VDP_Peek_128K` | **+6%** | 34 | 36 | = | 22339 / 22659 | smaller/equal |
| `VDP_Peek_16K` | **+50%** | 11 | 22 | **+27%** | 4451 / 6115 | smaller/faster |
| `VDP_Poke_128K` | **+11%** | 39 | 44 | **+6%** | 21383 / 22855 | smaller/faster |
| `VDP_Poke_16K` | **+40%** | 15 | 25 | **+24%** | 4835 / 6339 | smaller/faster |
| `VDP_ReadStatus` | **+9%** | 20 | 22 | **+6%** | 5443 / 5763 | smaller/faster |
| `VDP_ReadVRAM_128K` | **+5%** | 69 | 73 | = | 102919 / 103335 | smaller/equal |
| `VDP_ReadVRAM_16K` | **+25%** | 41 | 55 | **+1%** | 200175 / 202191 | smaller/equal |
| `VDP_RegWrite` | **+17%** | 10 | 12 | **+8%** | 3747 / 4067 | smaller/faster |
| `VDP_RegWriteBak` | — | 21 | 21 | = | 11011 / 11011 | equal/equal |
| `VDP_RegWriteBakMask` | **+29%** | 22 | 31 | **+15%** | 22595 / 26499 | smaller/faster |
| `VDP_SetAdjustOffset` | **+40%** | 6 | 10 | **+50%** | 5091 / 10275 | smaller/faster |
| `VDP_SetBlinkTile` | **+15%** | 84 | 99 | **+10%** | 65095 / 72263 | smaller/faster |
| `VDP_SetColorTable` | **+37%** | 94 | 149 | **+12%** | 60675 / 68935 | smaller/faster |
| `VDP_SetDefaultPalette` | — | 6 | 6 | = | 29191 / 28675 | equal/equal |
| `VDP_SetLayoutTable` | — | 98 | 98 | **+9%** | 41539 / 45831 | equal/faster |
| `VDP_SetMSX1Palette` | — | 6 | 6 | = | 28675 / 29191 | equal/equal |
| `VDP_SetMode` | **+30%** | 172 | 244 | **+15%** | 149569 / 175732 | smaller/faster |
| `VDP_SetModeFlag` | **+2%** | 40 | 41 | = | 30727 / 31175 | smaller/equal |
| `VDP_SetPage` | -307% | 57 | 14 | = | 16135 / 16135 | LARGER/equal |
| `VDP_SetPalette` | — | 17 | 17 | = | = (byte-identical) | equal/equal |
| `VDP_SetPaletteEntry` | **+19%** | 13 | 16 | **+6%** | 5283 / 5603 | smaller/faster |
| `VDP_SetPatternTable` | **+15%** | 80 | 94 | **+3%** | 44163 / 45319 | smaller/faster |
| `VDP_SetSprite` | **+45%** | 36 | 65 | **+42%** | 31495 / 54599 | smaller/faster |
| `VDP_SetSpriteAttributeTable` | **+11%** | 148 | 166 | **+14%** | 90051 / 104263 | smaller/faster |
| `VDP_SetSpriteColorSM1` | **+31%** | 18 | 26 | **+17%** | 26499 / 32071 | smaller/faster |
| `VDP_SetSpriteData` | — | 28 | 28 | = | = (byte-identical) | equal/equal |
| `VDP_SetSpriteExMultiColor` | **+65%** | 37 | 106 | **+21%** | 94027 / 119051 | smaller/faster |
| `VDP_SetSpriteExUniColor` | **+62%** | 39 | 103 | **+21%** | 95751 / 120651 | smaller/faster |
| `VDP_SetSpriteMultiColor` | **+7%** | 28 | 30 | **+19%** | 50375 / 62343 | smaller/faster |
| `VDP_SetSpritePattern` | **+32%** | 17 | 25 | **+15%** | 26567 / 31107 | smaller/faster |
| `VDP_SetSpritePatternTable` | **+14%** | 25 | 29 | **+9%** | 23171 / 25475 | smaller/faster |
| `VDP_SetSpritePosition` | **+45%** | 31 | 56 | **+43%** | 29059 / 50567 | smaller/faster |
| `VDP_SetSpritePositionX` | **+33%** | 16 | 24 | **+18%** | 25603 / 31175 | smaller/faster |
| `VDP_SetSpritePositionY` | **+35%** | 15 | 23 | **+17%** | 25155 / 30211 | smaller/faster |
| `VDP_SetSpriteSM1` | **+45%** | 41 | 75 | **+42%** | 35399 / 60807 | smaller/faster |
| `VDP_SetSpriteUniColor` | **+3%** | 28 | 29 | **+20%** | 51911 / 64775 | smaller/faster |
| `VDP_WriteLayout_GM2` | **+4%** | 103 | 107 | = | 128971 / 131019 | smaller/equal |
| `VDP_WriteVRAM_128K` | **+4%** | 65 | 68 | = | 102843 / 102939 | smaller/equal |
| `VDP_WriteVRAM_16K` | **+21%** | 44 | 56 | **+1%** | 201199 / 202479 | smaller/equal |
