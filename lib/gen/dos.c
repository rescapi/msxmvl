// msxmvl — clean-room reimplementation of MSXgl "dos" module.
// Toolchain: SDCC --sdcccall 1
//   arg1 -> HL(16b)/A(8b), arg2 -> DE(16b)/E(8b), arg3+ -> stack.
//   return 8b->A, 16b->HL, 32b->DE:HL.
//
// Every wrapper marshals its C arguments into the register layout expected by
// the MSX-DOS BDOS entry (0x0005) — function selector in C — then issues the
// call and returns the documented result. The simple FCB/handle calls emit no
// frame-register guard: those BDOS functions leave IX/IY intact, so a push/pop
// would be wasted bytes + cycles.
//
// EXCEPTION — function 0x1B (disk allocation info: DOS_GetFreeSpace /
// DOS_GetDiskInfo). It performs real disk I/O and, unguarded, returns error
// (A = 0xFF) instead of the allocation data. Those two DO push/pop IX around
// the call; it is load-bearing, not decoration. Verified against the openMSX
// MSX-DOS 2.20 arbiter (removing it flips sectors-per-cluster 0x02 -> 0xFF).
// Do not "optimise" it away.
//
// NOTE: these routines require a live MSX-DOS environment to execute (and the
// file/directory ones a mounted disk); see the driver's harness notes. They
// still link and are byte-callable in any build.

#include "dos.h"

//=============================================================================
// GLOBALS
//=============================================================================

#if ((DOS_USE_VALIDATOR) || (DOS_USE_ERROR_HANDLER))
u8  g_DOS_LastError;
u16 g_DOS_ErrorStack;
#endif

// Last file info block, also used as the DTA target for FFIRST/FNEXT.
DOS_FIB g_DOS_LastFIB;

#if (DOS_USE_ERROR_HANDLER)
// RAM landing area for the installed error handler code.
static u8 g_DOS_ErrorHandler[64];
// MSX-DOS 1 disk-error vector. DOS dispatches disk errors through M_DISKVE, which must
// hold the address of a WORD pointing at the handler (double indirection, as MSXgl does:
// DISKVE -> &pointer -> handler). Without patching it the installed handler never fires.
static void* g_DOS_ErrorHandlerPtr = g_DOS_ErrorHandler;
#ifndef M_DISKVE
	#define M_DISKVE	0xF323
#endif
#endif

#if (DOS_USE_UTILITIES)
// Backing store for DOS_GetTime().
static DOS_Time g_DOS_Time;
#endif

//=============================================================================
// MSX-DOS 1 CORE
//=============================================================================

// DOS_Call(func=A): C=func, issue BDOS.
void DOS_Call(u8 func) __naked
{
	(void)func;
	__asm
		ld   c, a
		call 0x0005
		ret
	__endasm;
}

// DOS_Exit0(): TERM0 (0x00) — resets the system, never returns.
void DOS_Exit0() __naked
{
	__asm
		ld   c, #0x00
		jp   0x0005          ; TERM0 never returns -> tail call (saves 1B vs call+ret)
	__endasm;
}

//=============================================================================
// MSX-DOS 1 CONSOLE IO
//=============================================================================

// DOS_CharInput(): CONIN (0x01) -> char in A.
c8 DOS_CharInput() __naked
{
	__asm
		ld   c, #0x01
		call 0x0005
		ret
	__endasm;
}

// DOS_CharOutput(chr=A): CONOUT (0x02), char in E.
void DOS_CharOutput(c8 chr) __naked
{
	(void)chr;
	__asm
		ld   e, a
		ld   c, #0x02
		call 0x0005
		ret
	__endasm;
}

// DOS_StringOutput(str=HL): STROUT (0x09), string ('$'-terminated) in DE.
void DOS_StringOutput(const c8* str) __naked
{
	(void)str;
	__asm
		ex   de, hl
		ld   c, #0x09
		call 0x0005
		ret
	__endasm;
}

//=============================================================================
// MSX-DOS 1 FILE HANDLING (FCB)
//=============================================================================
#if (DOS_USE_FCB)

// DOS_SetTransferAddr(data=HL): SETDTA (0x1A), address in DE.
void DOS_SetTransferAddr(void* data) __naked
{
	(void)data;
	__asm
		ex   de, hl
		ld   c, #0x1A
		call 0x0005
		ret
	__endasm;
}

// DOS_GetFreeSpace(drive=A): BDOS function 0x1B (get disk allocation info). Input E = drive;
// the call returns free CLUSTERS (not sectors) in HL, moved to DE for the --sdcccall 1 return.
//
// THIS ONE NEEDS THE IX GUARD, unlike the FCB wrappers above. The module header's blanket "BDOS
// preserves IX/IY" holds for the simple FCB calls but NOT for 0x1B: it does real disk I/O, and
// with IX left exposed on the stack the call returns error (A = 0xFF) instead of the allocation
// data. Verified against the openMSX MSX-DOS 2.20 arbiter -- drop the push/pop and the returned
// sectors-per-cluster flips 0x02 -> 0xFF. So the guard is load-bearing here, not decoration.
u16 DOS_GetFreeSpace(u8 drive) __naked
{
	(void)drive;
	__asm
		push ix             ; 0x1B corrupts across the disk call -- see note above
		ld   e, a           ; E = drive
		ld   c, #0x1B
		call 0x0005
		pop  ix
		ex   de, hl         ; free clusters HL -> DE (--sdcccall 1 returns u16 in DE)
		ret
	__endasm;
}

// Shared FCB-in-DE, return A pattern is expanded per function below.

// DOS_OpenFCB(stream=HL): FOPEN (0x0F).
u8 DOS_OpenFCB(DOS_FCB* stream) __naked
{
	(void)stream;
	__asm
		ex   de, hl
		ld   c, #0x0F
#if ((DOS_USE_VALIDATOR) || (DOS_USE_ERROR_HANDLER))
		ld   (_g_DOS_ErrorStack), sp
#endif
		call 0x0005
#if ((DOS_USE_VALIDATOR) || (DOS_USE_ERROR_HANDLER))
		ld   (_g_DOS_LastError), a
#endif
		ret
	__endasm;
}

// DOS_CloseFCB(stream=HL): FCLOSE (0x10).
u8 DOS_CloseFCB(DOS_FCB* stream) __naked
{
	(void)stream;
	__asm
		ex   de, hl
		ld   c, #0x10
#if ((DOS_USE_VALIDATOR) || (DOS_USE_ERROR_HANDLER))
		ld   (_g_DOS_ErrorStack), sp
#endif
		call 0x0005
#if ((DOS_USE_VALIDATOR) || (DOS_USE_ERROR_HANDLER))
		ld   (_g_DOS_LastError), a
#endif
		ret
	__endasm;
}

// DOS_CreateFCB(stream=HL): FMAKE (0x16).
u8 DOS_CreateFCB(DOS_FCB* stream) __naked
{
	(void)stream;
	__asm
		ex   de, hl
		ld   c, #0x16
#if ((DOS_USE_VALIDATOR) || (DOS_USE_ERROR_HANDLER))
		ld   (_g_DOS_ErrorStack), sp
#endif
		call 0x0005
#if ((DOS_USE_VALIDATOR) || (DOS_USE_ERROR_HANDLER))
		ld   (_g_DOS_LastError), a
#endif
		ret
	__endasm;
}

// DOS_DeleteFCB(stream=HL): FDEL (0x13).
u8 DOS_DeleteFCB(DOS_FCB* stream) __naked
{
	(void)stream;
	__asm
		ex   de, hl
		ld   c, #0x13
#if ((DOS_USE_VALIDATOR) || (DOS_USE_ERROR_HANDLER))
		ld   (_g_DOS_ErrorStack), sp
#endif
		call 0x0005
#if ((DOS_USE_VALIDATOR) || (DOS_USE_ERROR_HANDLER))
		ld   (_g_DOS_LastError), a
#endif
		ret
	__endasm;
}

// DOS_SequentialReadFCB(stream=HL): RDSEQ (0x14).
u8 DOS_SequentialReadFCB(DOS_FCB* stream) __naked
{
	(void)stream;
	__asm
		ex   de, hl
		ld   c, #0x14
#if ((DOS_USE_VALIDATOR) || (DOS_USE_ERROR_HANDLER))
		ld   (_g_DOS_ErrorStack), sp
#endif
		call 0x0005
#if ((DOS_USE_VALIDATOR) || (DOS_USE_ERROR_HANDLER))
		ld   (_g_DOS_LastError), a
#endif
		ret
	__endasm;
}

// DOS_SequentialWriteFCB(stream=HL): WRSEQ (0x15).
u8 DOS_SequentialWriteFCB(DOS_FCB* stream) __naked
{
	(void)stream;
	__asm
		ex   de, hl
		ld   c, #0x15
#if ((DOS_USE_VALIDATOR) || (DOS_USE_ERROR_HANDLER))
		ld   (_g_DOS_ErrorStack), sp
#endif
		call 0x0005
#if ((DOS_USE_VALIDATOR) || (DOS_USE_ERROR_HANDLER))
		ld   (_g_DOS_LastError), a
#endif
		ret
	__endasm;
}

// DOS_RandomBlockWriteFCB(stream=HL, records=DE): WRBLK (0x26) — FCB in DE, count in HL.
u8 DOS_RandomBlockWriteFCB(DOS_FCB* stream, u16 records) __naked
{
	(void)stream; (void)records;
	__asm
		ex   de, hl         ; DE = FCB, HL = record count
		ld   c, #0x26
#if ((DOS_USE_VALIDATOR) || (DOS_USE_ERROR_HANDLER))
		ld   (_g_DOS_ErrorStack), sp
#endif
		call 0x0005
#if ((DOS_USE_VALIDATOR) || (DOS_USE_ERROR_HANDLER))
		ld   (_g_DOS_LastError), a
#endif
		ret
	__endasm;
}

// DOS_RandomBlockReadFCB(stream=HL, records=DE): RDBLK (0x27) — returns records read in HL.
u16 DOS_RandomBlockReadFCB(DOS_FCB* stream, u16 records) __naked
{
	(void)stream; (void)records;
	__asm
		ex   de, hl         ; DE = FCB, HL = record count
		ld   c, #0x27
#if ((DOS_USE_VALIDATOR) || (DOS_USE_ERROR_HANDLER))
		ld   (_g_DOS_ErrorStack), sp
#endif
		call 0x0005
#if ((DOS_USE_VALIDATOR) || (DOS_USE_ERROR_HANDLER))
		ld   (_g_DOS_LastError), a
#endif
		ex   de, hl         ; SDCC --sdcccall 1 returns u16 in DE, not HL
		ret                 ; DE = number of records actually read
	__endasm;
}

// DOS_FindFirstFileFCB(stream=HL): SFIRST (0x11).
u8 DOS_FindFirstFileFCB(DOS_FCB* stream) __naked
{
	(void)stream;
	__asm
		ex   de, hl
		ld   c, #0x11
#if ((DOS_USE_VALIDATOR) || (DOS_USE_ERROR_HANDLER))
		ld   (_g_DOS_ErrorStack), sp
#endif
		call 0x0005
#if ((DOS_USE_VALIDATOR) || (DOS_USE_ERROR_HANDLER))
		ld   (_g_DOS_LastError), a
#endif
		ret
	__endasm;
}

// DOS_FindNextFileFCB(): SNEXT (0x12).
u8 DOS_FindNextFileFCB() __naked
{
	__asm
		ld   c, #0x12
#if ((DOS_USE_VALIDATOR) || (DOS_USE_ERROR_HANDLER))
		ld   (_g_DOS_ErrorStack), sp
#endif
		call 0x0005
#if ((DOS_USE_VALIDATOR) || (DOS_USE_ERROR_HANDLER))
		ld   (_g_DOS_LastError), a
#endif
		ret
	__endasm;
}

#endif // (DOS_USE_FCB)

//=============================================================================
// MSX-DOS 1 ERROR HANDLING
//=============================================================================
#if (DOS_USE_ERROR_HANDLER)

// DOS_InstallErrorHandler(cb=HL, size=E): copy the handler body into a page-3
// RAM landing area, record the current stack pointer for the handler to restore,
// then patch the MSX-DOS disk-error vector (M_DISKVE) to dispatch to it -- matching
// MSXgl, which otherwise leaves the handler installed but never called.
void DOS_InstallErrorHandler(callback cb, u8 size) __naked
{
	(void)cb; (void)size;
	__asm
		; `size` is a trailing u8 => passed ON THE STACK by SDCC --sdcccall 1, not in E.
		push hl                 ; save cb source
		ld   hl, #4             ; saved cb(2) + ret(2) -> size
		add  hl, sp
		ld   c, (hl)            ; C = size
		inc  hl                 ; -> caller SP (past the 1-byte arg)
		ld   (_g_DOS_ErrorStack), hl
		pop  hl                 ; HL = cb source
		ld   de, #_g_DOS_ErrorHandler
		ld   b, #0              ; BC = size
		ld   a, c
		or   a
		jr   z, 1$
		ldir
	1$:
		; Point the indirection word at the handler landing area AT RUNTIME (the static
		; initializer can't be relied on -- a ROM crt0 need not copy initialized .data to
		; RAM), then patch the MSX-DOS disk-error vector to it. DISKVE -> &ptr -> handler,
		; matching MSXgl (which likewise assigns g_DOS_ErrorHandler = ptr before Poke16).
		ld   hl, #_g_DOS_ErrorHandler
		ld   (_g_DOS_ErrorHandlerPtr), hl
		ld   hl, #_g_DOS_ErrorHandlerPtr
		ld   (M_DISKVE), hl
		pop  hl                 ; HL = return address
		inc  sp                 ; discard the 1-byte size arg (callee-clean)
		jp   (hl)
	__endasm;
}

#endif // (DOS_USE_ERROR_HANDLER)

//=============================================================================
// MSX-DOS 1 MISC
//=============================================================================

// DOS_GetVersion(ver=HL): DOSVER (0x6F). Fills {Kernel=BC, System=DE}; returns B.
u8 DOS_GetVersion(DOS_Version* ver) __naked
{
	(void)ver;
	__asm
		push hl                 ; save ver ptr
		ld   c, #0x6F
		call 0x0005
		pop  hl                 ; HL = ver ptr
		; A = err, B:C = kernel, D:E = system
		or   a
		ret  nz                 ; on error, leave the struct untouched
		ld   (hl), c            ; Kernel low
		inc  hl
		ld   (hl), b            ; Kernel high
		inc  hl
		ld   (hl), e            ; System low
		inc  hl
		ld   (hl), d            ; System high
		ld   a, b               ; return kernel high part
		ret
	__endasm;
}

// DOS_SelectDrive(drive=A): SELDSK (0x0E) — returns number of drives.
u8 DOS_SelectDrive(u8 drive) __naked
{
	(void)drive;
	__asm
		ld   e, a
		ld   c, #0x0E
		call 0x0005
		ld   a, l               ; number of drives
		ret
	__endasm;
}

// DOS_AvailableDrives(): LOGIN (0x18) — returns drive bit field (low byte).
u8 DOS_AvailableDrives() __FASTCALL __naked
{
	__asm
		ld   c, #0x18
		call 0x0005
		ld   a, l
		ret
	__endasm;
}

// DOS_GetCurrentDrive(): CURDRV (0x19) — returns current drive in A.
u8 DOS_GetCurrentDrive() __naked
{
	__asm
		ld   c, #0x19
		call 0x0005
		ret
	__endasm;
}

// DOS_GetDiskInfo(drive=A, info=DE): BDOS function 0x1B, unpacked into a DOS_DiskInfo.
// Same call as DOS_GetFreeSpace, but here every returned register is kept: A = sectors per
// cluster, BC = sector size, DE = total clusters, HL = free clusters. The info pointer arrives
// in DE (SDCC arg2) and DE is also a return register of the call, so it is parked on the stack
// across the call and reclaimed into IY for the indexed stores.
//
// The IX guard is REQUIRED, for the same reason as DOS_GetFreeSpace: 0x1B does disk I/O and
// returns error (A = 0xFF, so SectorPerCluster reads 0xFF) if IX is left exposed. Verified vs
// the openMSX arbiter -- with the guard the fields read 0x02/512/... , without it 0xFF/garbage.
void DOS_GetDiskInfo(u8 drive, DOS_DiskInfo* info) __naked
{
	(void)drive; (void)info;
	__asm
		push de                 ; park info ptr (DE is a return register of the call)
		ld   e, a               ; E = drive
		ld   c, #0x1B
		push ix                 ; load-bearing: 0x1B errors without it (see note above)
		call 0x0005
		pop  ix
		pop  iy                 ; IY = info ptr
		; A = sectors/cluster, BC = sector size, DE = total clusters, HL = free clusters
		ld   0(iy), a           ; [0] SectorPerCluster
		ld   1(iy), c           ; [1] SectorSize low
		ld   2(iy), b           ; [2] SectorSize high
		ld   3(iy), e           ; [3] TotalClusters low
		ld   4(iy), d           ; [4] high
		ld   5(iy), l           ; [5] FreeClusters low
		ld   6(iy), h           ; [6] high
		ret
	__endasm;
}

//=============================================================================
// MSX-DOS 2 FILE HANDLING (HANDLE)
//=============================================================================
#if (DOS_USE_HANDLE)

// DOS_OpenHandle(path=HL, mode=E): OPEN (0x43) — returns handle (B).
// DOS_OpenHandle(path=HL, mode@stack): OPEN (0x43) — returns handle (B).
// Per SDCC --sdcccall 1 the trailing u8 `mode` is passed ON THE STACK (push af/inc sp),
// NOT in E. Reading it from E fed BDOS a garbage open mode -- and since the mode byte is
// a DENY mask (b0 = no write, b1 = no read), garbage with b1 set made every subsequent
// DOS_ReadHandle fail with .ACCV (0xC6). The 1-byte arg is callee-cleaned (inc sp).
u8 DOS_OpenHandle(const c8* path, u8 mode) __naked
{
	(void)path; (void)mode;
	__asm
		ex   de, hl             ; DE = path
		ld   hl, #2             ; ret(2) -> mode
		add  hl, sp
		ld   a, (hl)            ; A = mode
		ld   c, #0x43
		call 0x0005
		pop  hl                 ; HL = return address
		inc  sp                 ; discard the 1-byte stack arg (callee-clean)
#if ((DOS_USE_VALIDATOR) || (DOS_USE_ERROR_HANDLER))
		ld   (_g_DOS_LastError), a
#endif
		or   a                  ; BDOS error code in A
		jr   z, 1$
		ld   a, #0xFF           ; error -> invalid handle
		jp   (hl)
	1$:
		ld   a, b               ; return file handle
		jp   (hl)
	__endasm;
}

// DOS_CreateHandle(path=HL, mode@stack, attr@stack): BDOS function 0x44 (create/open file).
// Derived from the SDCC --sdcccall 1 layout: the first pointer arg is in HL, and the two
// trailing u8s (mode, attr) share one 2-byte stack slot (mode = low, attr = high). BDOS wants
// DE = path, A = mode, B = attr, C = 0x44, and returns the new handle in B (0 = error in A).
// Reclaiming the return address and the two bytes with pop iy / pop bc does the callee-cleanup
// the ABI requires and lands mode/attr in C/B in a single pop; the ret goes back through IY.
u8 DOS_CreateHandle(const c8* path, u8 mode, u8 attr) __naked
{
	(void)path; (void)mode; (void)attr;
	__asm
		pop  iy                 ; return address
		ex   de, hl             ; DE = path
		pop  bc                 ; C = mode, B = attr (callee-cleanup of 2 stack bytes)
		ld   a, c               ; A = mode
		ld   c, #0x44
		call 0x0005
#if (DOS_USE_VALIDATOR)
		ld   (_g_DOS_LastError), a
		or   a                  ; BDOS error code in A
		jp   z, fcreat_ok
		ld   a, #0xFF           ; error -> invalid handle
		jp   fcreat_end
#endif
	fcreat_ok:
		ld   a, b               ; return file handle
	fcreat_end:
		jp   (iy)
	__endasm;
}

// DOS_CloseHandle(file=A): CLOSE (0x45).
u8 DOS_CloseHandle(u8 file) __naked
{
	(void)file;
	__asm
		ld   b, a
		ld   c, #0x45
		call 0x0005
#if ((DOS_USE_VALIDATOR) || (DOS_USE_ERROR_HANDLER))
		ld   (_g_DOS_LastError), a
#endif
		ret
	__endasm;
}

// DOS_EnsureHandle(file=A): ENSURE (0x46).
u8 DOS_EnsureHandle(u8 file) __naked
{
	(void)file;
	__asm
		ld   b, a
		ld   c, #0x46
		call 0x0005
#if ((DOS_USE_VALIDATOR) || (DOS_USE_ERROR_HANDLER))
		ld   (_g_DOS_LastError), a
#endif
		ret
	__endasm;
}

// DOS_DuplicateHandle(file=A): DUP (0x47) — returns new handle (B).
u8 DOS_DuplicateHandle(u8 file) __naked
{
	(void)file;
	__asm
		ld   b, a
		ld   c, #0x47
		call 0x0005
#if ((DOS_USE_VALIDATOR) || (DOS_USE_ERROR_HANDLER))
		ld   (_g_DOS_LastError), a
#endif
		or   a                  ; BDOS error code in A
		jr   z, 1$
		ld   a, #0xFF           ; error -> invalid handle
		ret
	1$:
		ld   a, b               ; return new handle
		ret
	__endasm;
}

// DOS_ReadHandle(file=A, buffer=DE, size@stack): BDOS function 0x48 (read from handle).
// Derived from the ABI: handle in A (SDCC arg1), buffer already in DE (arg2), and the u16 size
// on the stack (arg3). BDOS wants B = handle, DE = buffer, HL = size, C = 0x48, and returns the
// byte count in HL. pop iy / pop hl grabs the return address and the size in one sweep (the
// callee-cleanup the ABI mandates), then the result HL is moved to DE for the u16 return.
u16 DOS_ReadHandle(u8 file, void* buffer, u16 size) __naked
{
	(void)file; (void)buffer; (void)size;
	__asm
		ld   b, a               ; B = file handle (DE already = buffer)
		pop  iy                 ; return address
		pop  hl                 ; HL = size (callee-cleanup of 2 stack bytes)
		ld   c, #0x48
		call 0x0005
		ex   de, hl             ; SDCC --sdcccall 1 returns u16 in DE, not HL
#if (DOS_USE_VALIDATOR)
		ld   (_g_DOS_LastError), a
		or   a                  ; BDOS error code in A
		jp   z, fread_end
		ld   e, #0xFF
		ld   d, e               ; error -> 0xFFFF
#endif
	fread_end:
		jp   (iy)               ; return DE = bytes read
	__endasm;
}

// DOS_WriteHandle(file=A, buffer=DE, size@stack): WRITE (0x49) — returns bytes written (HL).
u16 DOS_WriteHandle(u8 file, const void* buffer, u16 size) __naked
{
	(void)file; (void)buffer; (void)size;
	__asm
		pop  iy                 ; return address
		ld   b, a
		pop  hl                 ; HL = size (callee-cleanup of 2 stack bytes)
		ld   c, #0x49
		call 0x0005
		ex   de, hl             ; SDCC --sdcccall 1 returns u16 in DE, not HL
#if (DOS_USE_VALIDATOR)
		ld   (_g_DOS_LastError), a
		or   a                  ; BDOS error code in A
		jp   z, fwrite_end
		ld   e, #0xFF
		ld   d, e               ; error -> 0xFFFF
#endif
	fwrite_end:
		jp   (iy)               ; return DE = bytes written
	__endasm;
}

// DOS_SeekHandle(file=A, offset@stack:32, mode@stack:8): SEEK (0x4A) __CALLEE — returns DE:HL.
// z88dk_callee layout (verified): [ret][offset_lo][offset_hi][mode] with file in A.
u32 DOS_SeekHandle(u8 file, i32 offset, u8 mode) __CALLEE __naked
{
	(void)file; (void)offset; (void)mode;
	__asm
		pop  iy                 ; return address
		ld   b, a               ; B = file handle
		pop  hl                 ; offset low word
		pop  de                 ; offset high word
		dec  sp                 ; adjust for the 1-byte mode arg
		pop  af                 ; A = mode (method); cleans the 5 arg bytes total
		ld   c, #0x4A
		call 0x0005
		ex   de, hl             ; DE:HL => HL:DE
#if (DOS_USE_VALIDATOR)
		ld   (_g_DOS_LastError), a
		or   a
		jp   z, flseek_end
		ld   e, #0xFF
		ld   d, e
		ld   l, e
		ld   h, e               ; error -> 0xFFFFFFFF
#endif
	flseek_end:
		jp   (iy)               ; return HL:DE (DE:HL result)
	__endasm;
}

// DOS_DeleteHandle(file=A): HDELETE (0x52).
u8 DOS_DeleteHandle(u8 file) __naked
{
	(void)file;
	__asm
		ld   b, a
		ld   c, #0x52
		call 0x0005
#if ((DOS_USE_VALIDATOR) || (DOS_USE_ERROR_HANDLER))
		ld   (_g_DOS_LastError), a
#endif
		ret
	__endasm;
}

// DOS_RenameHandle(file=A, newPath=DE): HRENAME (0x53).
u8 DOS_RenameHandle(u8 file, const c8* newPath) __naked
{
	(void)file; (void)newPath;
	__asm
		ld   b, a               ; B = file (DE already = new path)
		ld   c, #0x53
		call 0x0005
#if ((DOS_USE_VALIDATOR) || (DOS_USE_ERROR_HANDLER))
		ld   (_g_DOS_LastError), a
#endif
		ret
	__endasm;
}

// DOS_MoveHandle(file=A, newPath=DE): HMOVE (0x54).
u8 DOS_MoveHandle(u8 file, const c8* newPath) __naked
{
	(void)file; (void)newPath;
	__asm
		ld   b, a
		ld   c, #0x54
		call 0x0005
#if ((DOS_USE_VALIDATOR) || (DOS_USE_ERROR_HANDLER))
		ld   (_g_DOS_LastError), a
#endif
		ret
	__endasm;
}

// DOS_SetAttributeHandle(file=A, attr=E): HATTR set (0x55), A=1.
u8 DOS_SetAttributeHandle(u8 file, u8 attr) __naked
{
	(void)file; (void)attr;
	__asm
		ld   b, a               ; B = file handle
		                        ; L already = attributes: for (u8,u8) SDCC --sdcccall 1
		                        ; passes arg1 in A and arg2 in L. Do NOT reload it from E.
		ld   a, #1              ; A = 1 (set)
		ld   c, #0x55
		call 0x0005
#if ((DOS_USE_VALIDATOR) || (DOS_USE_ERROR_HANDLER))
		ld   (_g_DOS_LastError), a
#endif
		or   a                  ; BDOS error code in A
		jr   z, 1$
		ld   a, #0xFF           ; error
		ret
	1$:
		ld   a, l               ; success
		ret
	__endasm;
}

// DOS_GetAttributeHandle(file=A): HATTR get (0x55), A=0 — returns attributes (L).
u8 DOS_GetAttributeHandle(u8 file) __naked
{
	(void)file;
	__asm
		ld   b, a               ; B = file handle
		xor  a                  ; A = 0 (get)
		ld   c, #0x55
		call 0x0005
#if ((DOS_USE_VALIDATOR) || (DOS_USE_ERROR_HANDLER))
		ld   (_g_DOS_LastError), a
#endif
		or   a                  ; BDOS error code in A
		jr   z, 1$
		ld   a, #0xFF           ; error
		ret
	1$:
		ld   a, l               ; return attributes byte
		ret
	__endasm;
}

#endif // (DOS_USE_HANDLE)

//=============================================================================
// MSX-DOS 2 CORE
//=============================================================================

// DOS_GetDiskParam(drv=A, param=DE): DPARM (0x31), L=drive, DE=buffer.
u8 DOS_GetDiskParam(u8 drv, DOS_DiskParam* param) __naked
{
	(void)drv; (void)param;
	__asm
		ld   l, a               ; L = drive number (DE already = buffer)
		ld   c, #0x31
		call 0x0005
#if ((DOS_USE_VALIDATOR) || (DOS_USE_ERROR_HANDLER))
		ld   (_g_DOS_LastError), a
#endif
		ret
	__endasm;
}

// DOS_Exit(err=A): TERM (0x62) — never returns.
void DOS_Exit(u8 err) __naked
{
	(void)err;
	__asm
		ld   b, a
		ld   c, #0x62
		jp   0x0005          ; TERM never returns -> tail call (saves 1B vs call+ret)
	__endasm;
}

// DOS_Explain(err=A, str=DE): EXPLAIN (0x66), B=err, DE=buffer.
void DOS_Explain(u8 err, c8* str) __naked
{
	(void)err; (void)str;
	__asm
		ld   b, a
		ld   c, #0x66
		call 0x0005
		ret
	__endasm;
}

//=============================================================================
// MSX-DOS 2 UTILITIES
//=============================================================================
#if (DOS_USE_UTILITIES)

// DOS_FindFirstEntry(filename=HL, attr=E): set DTA to g_DOS_LastFIB, FFIRST (0x40).
// Returns &g_DOS_LastFIB on success, NULL on error.
DOS_FIB* DOS_FindFirstEntry(const c8* filename, u8 attr) __naked
{
	(void)filename; (void)attr;
	__asm
		push hl                 ; save filename
		push de                 ; save attr (in E)
		ld   de, #_g_DOS_LastFIB
		ld   c, #0x1A           ; SETDTA
		call 0x0005
		pop  bc                 ; C = attr
		ld   b, c               ; B = attr
		pop  de                 ; DE = filename
		ld   c, #0x40           ; FFIRST
		call 0x0005
#if ((DOS_USE_VALIDATOR) || (DOS_USE_ERROR_HANDLER))
		ld   (_g_DOS_LastError), a
#endif
		or   a
		jr   nz, 1$
		ld   de, #_g_DOS_LastFIB ; SDCC --sdcccall 1 returns pointer in DE, not HL
		ret
	1$:
		ld   de, #0             ; NULL (in DE)
		ret
	__endasm;
}

// DOS_FindNextEntry(): reassert DTA then FNEXT (0x41).
DOS_FIB* DOS_FindNextEntry() __naked
{
	__asm
		ld   de, #_g_DOS_LastFIB
		ld   c, #0x1A           ; SETDTA
		call 0x0005
		ld   c, #0x41           ; FNEXT
		call 0x0005
#if ((DOS_USE_VALIDATOR) || (DOS_USE_ERROR_HANDLER))
		ld   (_g_DOS_LastError), a
#endif
		or   a
		jr   nz, 1$
		ld   de, #_g_DOS_LastFIB ; SDCC --sdcccall 1 returns pointer in DE, not HL
		ret
	1$:
		ld   de, #0             ; NULL (in DE)
		ret
	__endasm;
}

// DOS_Delete(path=HL): DELETE (0x4D), DE=path.
u8 DOS_Delete(const c8* path) __naked
{
	(void)path;
	__asm
		ex   de, hl
		ld   c, #0x4D
		call 0x0005
#if ((DOS_USE_VALIDATOR) || (DOS_USE_ERROR_HANDLER))
		ld   (_g_DOS_LastError), a
#endif
		ret
	__endasm;
}

// DOS_Rename(path=HL, newPath=DE): RENAME (0x4E), DE=old, HL=new.
u8 DOS_Rename(const c8* path, const c8* newPath) __naked
{
	(void)path; (void)newPath;
	__asm
		ex   de, hl             ; DE = old path, HL = new path
		ld   c, #0x4E
		call 0x0005
#if ((DOS_USE_VALIDATOR) || (DOS_USE_ERROR_HANDLER))
		ld   (_g_DOS_LastError), a
#endif
		ret
	__endasm;
}

// DOS_Move(path=HL, newPath=DE): MOVE (0x4F), DE=old, HL=new.
u8 DOS_Move(const c8* path, const c8* newPath) __naked
{
	(void)path; (void)newPath;
	__asm
		ex   de, hl
		ld   c, #0x4F
		call 0x0005
#if ((DOS_USE_VALIDATOR) || (DOS_USE_ERROR_HANDLER))
		ld   (_g_DOS_LastError), a
#endif
		ret
	__endasm;
}

// DOS_SetAttribute(path=HL, attr=E): ATTR set (0x50), DE=path, A=1, L=attr.
// DOS_SetAttribute(path=HL, attr@stack): ATTR set (0x50).
// Per SDCC --sdcccall 1 the trailing u8 `attr` is passed ON THE STACK, not in E
// (same trap as DOS_OpenHandle had). Callee-clean the 1-byte arg with `inc sp`.
u8 DOS_SetAttribute(const c8* path, u8 attr) __naked
{
	(void)path; (void)attr;
	__asm
		ex   de, hl             ; DE = path
		ld   hl, #2             ; ret(2) -> attr
		add  hl, sp
		ld   a, (hl)
		ld   l, a               ; L = attributes
		ld   a, #1              ; A = 1 (set)
		ld   c, #0x50
		call 0x0005
		pop  hl                 ; HL = return address
		inc  sp                 ; discard the 1-byte attr arg (callee-clean)
#if ((DOS_USE_VALIDATOR) || (DOS_USE_ERROR_HANDLER))
		ld   (_g_DOS_LastError), a
#endif
		or   a                  ; BDOS error code in A
		jr   z, 1$
		ld   a, #0xFF           ; error
		jp   (hl)
	1$:
		xor  a, a               ; success
		jp   (hl)
	__endasm;
}

// DOS_GetAttribute(path=HL): ATTR get (0x50), DE=path, A=0 — returns attributes (L).
u8 DOS_GetAttribute(const c8* path) __naked
{
	(void)path;
	__asm
		ex   de, hl             ; DE = path
		xor  a                  ; A = 0 (get)
		ld   c, #0x50
		call 0x0005
#if ((DOS_USE_VALIDATOR) || (DOS_USE_ERROR_HANDLER))
		ld   (_g_DOS_LastError), a
#endif
		or   a                  ; BDOS error code in A
		jr   z, 1$
		ld   a, #0xFF           ; error
		ret
	1$:
		ld   a, l               ; return attributes byte
		ret
	__endasm;
}

// DOS_GetDirectory(drive=A, path=DE): GETCD (0x59), B=drive, DE=buffer.
u8 DOS_GetDirectory(u8 drive, const c8* path) __naked
{
	(void)drive; (void)path;
	__asm
		ld   b, a               ; B = drive (DE already = buffer)
		ld   c, #0x59
		call 0x0005
#if ((DOS_USE_VALIDATOR) || (DOS_USE_ERROR_HANDLER))
		ld   (_g_DOS_LastError), a
#endif
		ret
	__endasm;
}

// DOS_ChangeDirectory(path=HL): CHDIR (0x5A), DE=path.
u8 DOS_ChangeDirectory(const c8* path) __naked
{
	(void)path;
	__asm
		ex   de, hl
		ld   c, #0x5A
		call 0x0005
#if ((DOS_USE_VALIDATOR) || (DOS_USE_ERROR_HANDLER))
		ld   (_g_DOS_LastError), a
#endif
		ret
	__endasm;
}

// DOS_GetTime(): GDATE (0x2A) + GTIME (0x2C) into g_DOS_Time; returns pointer.
const DOS_Time* DOS_GetTime() __naked
{
	__asm
		ld   c, #0x2A           ; GDATE
		call 0x0005
		ld   (_g_DOS_Time+0), hl   ; Year
		ld   (_g_DOS_Time+2), de    ; Date/Month
		ld   (_g_DOS_Time+4), a    ; Day (day of week)
		ld   c, #0x2C           ; GTIME
		call 0x0005
		ld   (_g_DOS_Time+5), hl    ; Minute/Hour
		ld   a, d
		ld   (_g_DOS_Time+7), a    ; Second
		ld   de, #_g_DOS_Time        ; SDCC --sdcccall 1 returns pointer in DE, not HL
		ret
	__endasm;
}

#endif // (DOS_USE_UTILITIES)

//=============================================================================
// BIOS INTER-SLOT (DOS-callable BIOS entries)
//=============================================================================
#if (DOS_USE_BIOSCALL)

// DOS_InterSlotRead(slot=A, addr=DE): RDSLT (0x000C), A=slot, HL=addr -> value in A.
u8 DOS_InterSlotRead(u8 slot, u16 addr) __naked
{
	(void)slot; (void)addr;
	__asm
		ex   de, hl             ; HL = addr (A = slot unchanged)
		call 0x000C             ; RDSLT does its own DI (and leaves ints off, per MSXgl)
		ret                     ; A = value read
	__endasm;
}

// DOS_InterSlotWrite(slot=A, addr=DE, value@stack): WRSLT (0x0014), A=slot, HL=addr, E=value.
void DOS_InterSlotWrite(u8 slot, u16 addr, u8 value) __naked
{
	(void)slot; (void)addr; (void)value;
	__asm
		ld   hl, #2             ; ret(2) => value
		add  hl, sp
		ld   b, (hl)            ; B = value
		ex   de, hl             ; HL = addr
		ld   e, b               ; E = value (A = slot unchanged)
		di
		call 0x0014
		ei
		pop  hl                 ; ret addr
		inc  sp                 ; discard the 1-byte value stack arg (callee-cleanup)
		jp   (hl)
	__endasm;
}

// DOS_InterSlotCall(slot=A, addr=DE): CALSLT (0x001C), IX=addr, IYh=slot.
void DOS_InterSlotCall(u8 slot, u16 addr) __naked
{
	(void)slot; (void)addr;
	__asm
		push iy                 ; preserve IY (CALSLT clobbers it)
		push de
		pop  ix                 ; IX = addr
		ld   d, a               ; D = slot
		ld   e, #0
		push de
		pop  iy                 ; IYH = slot (FIX: was a bare 'push de' -> IYH unset + stack leak)
		call 0x001C             ; CALSLT
		pop  iy
		ret
	__endasm;
}

#endif // (DOS_USE_BIOSCALL)
