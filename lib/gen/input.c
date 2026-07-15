// msxmvl — clean-room reimplementation of MSXgl "input" module (bodies).
// Signatures + documented behaviour from MSXgl engine/src/input.h only.
// No MSXgl implementation source was read.
//
// Calling convention (SDCC 4.x, --sdcccall 1):
//   plain    : arg1 8b->A  16b->HL  ; arg2->DE ; ret 8b->A 16b->HL
//   fastcall : arg  8b->L  16b->HL  ; ret 8b->L 16b->HL
//
// Hardware map:
//   PSG R#15 (port B, out) : b0-b3 pin6/7 of port1/2, b4/b5 pin8 of port1/2,
//                            b6 joystick-read select (0=port1,1=port2).
//   PSG R#14 (port A, in)  : b0=up b1=down b2=left b3=right b4=trigA b5=trigB.
//   PPI  0xAA (port C)     : low nibble = keyboard matrix row select.
//   PPI  0xA9 (port B, in) : 8 column bits of selected row (0=pressed).
#include "input.h"

// MSX I/O port addresses referenced from the __asm blocks below. Defined locally
// (guarded) so the module is self-contained for standalone msxmvl builds; when
// compiled against MSXgl's headers these already come from system_port.h, so the
// guards yield to those. The C preprocessor expands them inside __asm before the
// assembler runs, matching how MSXgl resolves the same symbols.
#ifndef P_PSG_REGS
	#define P_PSG_REGS	0xA0	// PSG register-select (write)
#endif
#ifndef P_PSG_DATA
	#define P_PSG_DATA	0xA1	// PSG data (write)
#endif
#ifndef P_PSG_STAT
	#define P_PSG_STAT	0xA2	// PSG data (read)
#endif
#ifndef P_PPI_B
	#define P_PPI_B		0xA9	// PPI port B (keyboard row read)
#endif
#ifndef P_PPI_C
	#define P_PPI_C		0xAA	// PPI port C (keyboard row select)
#endif

//=============================================================================
// Group: Joystick
//=============================================================================
#if (INPUT_USE_JOYSTICK || INPUT_USE_DETECT)

// FASTCALL: port in L, return in L. Only A/F/L (and IX) may be clobbered;
// B,C,D,E,H,IY are preserved as declared (untouched below).
u8 Joystick_Read(u8 port) __FASTCALL __NAKED __PRESERVES(b, c, d, e, h, iyl, iyh)
{
	port;
	__asm
		INPUT_DI
		ld		a, #15
		out		(P_PSG_REGS), a		; select R#15 (port B, output)
		ld		a, l				; A = requested port value
		out		(P_PSG_DATA), a		; write select + pin outputs
		ld		a, #14
		out		(P_PSG_REGS), a		; select R#14 (port A, input)
		in		a, (P_PSG_STAT)		; read pins -> xxBARLDU (0=pressed)
		ld		l, a				; return in L
		INPUT_EI
		ret
	__endasm;
}

#endif // (INPUT_USE_JOYSTICK || INPUT_USE_DETECT)

#if (INPUT_USE_JOYSTICK || INPUT_USE_DETECT)
#if (INPUT_JOY_UPDATE)

u8 g_JoyStats[2] = { 0x3F, 0x3F };
u8 g_JoyStatsPrev[2] = { 0x3F, 0x3F };

// Snapshot previous frame, then read both ports.
void Joystick_Update()
{
	g_JoyStatsPrev[0] = g_JoyStats[0];
	g_JoyStats[0] = ~Joystick_Read(JOY_PORT_1);
	g_JoyStatsPrev[1] = g_JoyStats[1];
	g_JoyStats[1] = ~Joystick_Read(JOY_PORT_2);
}

#endif // (INPUT_JOY_UPDATE)
#endif

//=============================================================================
// Shared PSG helper: set R#15 then read R#14 (port A). Used by Detect / Mouse.
//=============================================================================
#if (INPUT_USE_DETECT || INPUT_USE_MOUSE)

// PSG I/O ports as sfr lvalues. MSXgl only exposes g_PSG_*Port under the
// internal-PSG config (PSG_CHIP == PSG_INTERNAL); read them straight from the
// fixed hardware ports so this is config-independent.
__sfr __at(0xA0) g_mvc_PsgReg;    // PSG register select (write)
__sfr __at(0xA1) g_mvc_PsgData;   // PSG data (write)
__sfr __at(0xA2) g_mvc_PsgStat;   // PSG status (read)

static u8 PSG_SetSelectRead(u8 r15)
{
	__asm
		INPUT_DI
	__endasm;
	g_mvc_PsgReg = 15;
	g_mvc_PsgData = r15;
	g_mvc_PsgReg = 14;
	r15 = g_mvc_PsgStat;		// reuse arg slot for the read value
	__asm
		INPUT_EI
	__endasm;
	return r15;
}

#endif // (INPUT_USE_DETECT || INPUT_USE_MOUSE)

//=============================================================================
// Group: Detect
//=============================================================================
#if (INPUT_USE_DETECT)

// Select the port (bit6 of R#15) with pin8 high, read the 6 data pins in their
// idle state; the resulting signature approximates the connected device type.
u8 Input_Detect(enum INPUT_PORT port)
{
	return PSG_SetSelectRead((u8)port) & 0x3F;
}

#endif // (INPUT_USE_DETECT)

//=============================================================================
// Group: Mouse
//=============================================================================
#if (INPUT_USE_MOUSE)

// The MSX mouse is slow: it needs settling time between strobing R#15 and sampling the
// nibble on R#14, or the reads come back unreliable on real hardware. MSXgl waits WAIT2
// iterations before the FIRST read and WAIT1 before the rest; msxmvl previously omitted
// the wait entirely (which is why Mouse_Read benchmarked "faster" -- it was doing less).
// Iteration counts mirror MSXgl's input.c.
#define MOUSE_WAIT1		10
#define MOUSE_WAIT2		30

// MSX mouse strobe protocol: pin 8 (R#15 bits 4-5, INPUT_PIN8_ONLY) is toggled as a
// clock; after each edge the mouse presents a 4-bit nibble on R#14's low nibble. Four
// nibbles give the signed X then Y deltas; the buttons ride the Y-high read.
//
// Single __naked routine (no per-strobe C call) so it is not larger/slower than MSXgl's
// hand-asm GTMOUS. Observable behaviour is byte-identical to the previous C version:
//   PrevButtons = old Buttons; dX = (Xhi<<4)|Xlo; Buttons = raw Yhi read; dY = (Yhi<<4)|Ylo.
// The mandatory inter-strobe settling delay (WAIT2 first, WAIT1 after) is preserved.
// ABI (sdcccall 1): port in A, data pointer in DE.
void Mouse_Read(u8 port, Mouse_State* data) __NAKED __PRESERVES(iyl, iyh)
{
	port; data;
	__asm
		ld    c, a              ; C = port (pin8 high)
		ld    a, (de)           ; old Buttons (offset 0)
		inc   de
		inc   de
		inc   de
		ld    (de), a           ; PrevButtons (offset 3) = old Buttons
		dec   de
		dec   de
		dec   de                ; DE -> offset 0

		; --- X high nibble: strobe pin8-high, long settle ---
		ld    l, c
		ld    b, #MOUSE_WAIT2
		call  mr_strobe$
		and   #0x0F
		rlca
		rlca
		rlca
		rlca
		ld    h, a              ; H = (Xhi & 0x0F) << 4
		; --- X low nibble: strobe pin8-low ---
		ld    a, c
		and   #(0xFF & ~INPUT_PIN8_ONLY)
		ld    l, a
		ld    b, #MOUSE_WAIT1
		call  mr_strobe$
		and   #0x0F
		or    a, h              ; dX = (Xhi<<4) | Xlo
		inc   de
		ld    (de), a           ; offset 1 = dX
		dec   de

		; --- Y high nibble (also the buttons read) ---
		ld    l, c
		ld    b, #MOUSE_WAIT1
		call  mr_strobe$
		ld    (de), a           ; offset 0 = Buttons = raw Yhi read
		and   #0x0F
		rlca
		rlca
		rlca
		rlca
		ld    h, a              ; H = (Yhi & 0x0F) << 4
		; --- Y low nibble ---
		ld    a, c
		and   #(0xFF & ~INPUT_PIN8_ONLY)
		ld    l, a
		ld    b, #MOUSE_WAIT1
		call  mr_strobe$
		and   #0x0F
		or    a, h              ; dY = (Yhi<<4) | Ylo
		inc   de
		inc   de
		ld    (de), a           ; offset 2 = dY
		ret

		; Strobe R#15 = L, wait B djnz iterations (settling), read R#14 -> A.
	mr_strobe$:
		INPUT_DI
		ld    a, #15
		out   (P_PSG_REGS), a
		ld    a, l
		out   (P_PSG_DATA), a   ; strobe
	mr_wait$:
		djnz  mr_wait$          ; ~13 T/iter settle, like MSXgl's WAITMS
		ld    a, #14
		out   (P_PSG_REGS), a
		in    a, (P_PSG_STAT)
		INPUT_EI
		ret
	__endasm;
}

#endif // (INPUT_USE_MOUSE)

//=============================================================================
// Group: Keyboard
//=============================================================================
#if (INPUT_USE_KEYBOARD)

// FASTCALL: row in L, return in L. Preserves B,C,D,E,H,IY (untouched).
// Selects the matrix row on PPI port C (keeping the high nibble: CAP LED /
// cassette / click), then reads the 8 column bits on PPI port B.
u8 Keyboard_Read(u8 row) __FASTCALL __NAKED __PRESERVES(b, c, d, e, h, iyl, iyh)
{
	row;
	__asm
		in		a, (P_PPI_C)
		and		#0xF0					; only change bits 0-3
		or		l						; take row number from L
		out		(P_PPI_C), a
		in		a, (P_PPI_B)			; read row into A
		ld		l, a
		ret
	__endasm;
}

#if (INPUT_KB_UPDATE)

// These MUST default to the BIOS keyboard-matrix work areas, as MSXgl does
// (g_NEWKEY / g_OLDKEY). Left in BSS they are NULL, so Keyboard_IsKeyPressed /
// Keyboard_IsKeyPushed dereference address 0 and read BIOS ROM opcodes instead of
// the key matrix -- which reads as "keys held down" and makes callers act on
// phantom key presses. Callers may still retarget them via Keyboard_SetBuffer().
#ifndef M_NEWKEY
	#define M_NEWKEY	0xFBE5	// 11 bytes: current status of each keyboard row
#endif
#ifndef M_OLDKEY
	#define M_OLDKEY	0xFBDA	// 11 bytes: previous status of each keyboard row
#endif
u8* g_InputBufferNew = (u8*)M_NEWKEY;
u8* g_InputBufferOld = (u8*)M_OLDKEY;

// Copy current->previous, then re-read every configured row into 'new'.
void Keyboard_Update()
{
	u8* inputBufferNew = g_InputBufferNew;
	u8* inputBufferOld = g_InputBufferOld;

	for (u8 i = INPUT_KB_UPDATE_MIN; i <= INPUT_KB_UPDATE_MAX; ++i)
	{
		*inputBufferOld++ = *inputBufferNew;
		*inputBufferNew++ = Keyboard_Read(i);
	}
}

// Pressed this frame and released the previous one.
bool Keyboard_IsKeyPushed(u8 key)
{
	u8 flag = 1 << KEY_IDX(key);
	u8 row = KEY_ROW(key);
	return ((g_InputBufferNew[row] & flag) == 0) && ((g_InputBufferOld[row] & flag) != 0);
}

#endif // (INPUT_KB_UPDATE)

#if (INPUT_KB_AS_JOYSTICK)

// Emulate a joystick from the keyboard: cursor keys (row 8) for directions,
// Space (row 8 bit 0) for trigger A, N (row 4 bit 3) for trigger B.
// Returns xxBARLDU with bit=0 meaning pressed, matching Joystick_Read.
u8 Keyboard_ReadAsJoystick() __NAKED
{
	__asm
	; Get keyboard row 8
		in		a, (P_PPI_C)
		and		#0b11110000				; only change bits 0-3
		ld		d, a					; backup PPI state
		or		#8						; set row number to 8
		out		(P_PPI_C), a

		in		a, (P_PPI_B)			; read row into A				[RDUL...1]
		rrca							;								[1RDUL...]
		rrca							;								[.1RDUL..]
		ld		b, a					; backup
		and		#0b00000100				; keep only Left				[-----L--]
		ld		c, a					; backup Left

		ld		a, b					; restore						[.1RDUL..]
		rrca							;								[..1RDUL.]
		rrca							;								[...1RDUL]
		ld		b, a					; backup
		and		#0b00011000				; keep only (1) and Right		[---1R---]
		or		a, c					; combine Left and Right		[---1RL--]
		ld		c, a					; backup (1) and Right

		ld		a, b					; restore						[...1RDUL]
		rrca							;								[L...1RDU]
		and		#0b00000011				; keep only Down & Up			[------DU]
		or		a, c					; combine with Up and Down		[---1RLDU]
		ld		b, a					; backup

	; Get keyboard row 4
		ld		a, d					; restore PPI state
		or		#4						; set row number to 4
		out		(P_PPI_C), a
		in		a, (P_PPI_B)			; read row into A				[RQPONMLK]
		and		#0b00111000				; keep only N					[--PON---]
		rlca							;								[-PON----]
		rlca							;								[PON-----]

	; Combine
		or		a, b					; Combine with previous result	[PON1RLDU]
		ret
	__endasm;
}

#endif // (INPUT_KB_AS_JOYSTICK)

#endif // (INPUT_USE_KEYBOARD)
