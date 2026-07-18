// msxmvl EXTENSION module: unlzsa2 -- runtime LZSA2 decompressor.
//
// Decompressor for the LZSA2 format by Emmanuel Marty (format only; independent
// implementation). Clean-room: the RAM decoder below was written from the LZSA2
// block-format spec -- the XYZ|LL|MMM token, nibble-packed extra literal/match
// lengths, the five match-offset encodings (5/9/13/16-bit + repeat) stored as
// NEGATIVE values, and the raw-block EOD marker. Emmanuel Marty's `lzsa`
// compressor and the spke/uniabis reference Z80 decoders were used only to learn
// the format and as a behavioural oracle; the code here is our own. See
// docs/examples/unlzsa2_DOC.md for the measured size/speed comparison.
//
// Toolchain: SDCC --sdcccall 1. arg1 (src) -> HL, arg2 (dst) -> DE, so UnLZSA2's
// naked body is the decoder itself -- no argument shuffling and no wrapper.
#include "unlzsa2.h"

//=============================================================================
// Decoder state (module-local; UnLZSA2 is not re-entrant).
//=============================================================================
// Kept in RAM DATA rather than as self-modifying code, so the decoder itself is
// ROM-safe. g_LZSA2_Off is the last match offset stored as a NEGATIVE 16-bit
// value: `add hl,de` with HL = offset yields dst-distance directly. g_LZSA2_Nib
// is the nibble reservoir (bit7 = a low nibble is pending in bits 0-3). g_LZSA2_Tok
// holds the current token across the literal copy, which clobbers BC.
u16 g_LZSA2_Off;   // last match offset (negative)
u8  g_LZSA2_Nib;   // pending-nibble store: b7 = ready, b0-3 = value
u8  g_LZSA2_Tok;   // current command token XYZ|LL|MMM

//=============================================================================
// RAM DECODER
//=============================================================================
// Register roles: HL = compressed source, DE = output cursor, BC = run length for
// LDIR, A = accumulator. Each command: token -> optional literal length -> literal
// bytes -> match offset -> optional match length -> match copy. A raw block ends
// on the EOD marker inside the extended match length (nibble 15 then byte 232).
void UnLZSA2(const u8* src, u8* dst) __naked
{
	(void)src; (void)dst;   // src in HL, dst in DE (sdcccall 1)
	__asm
		xor  a, a
		ld   (_g_LZSA2_Nib), a      ; start with no pending nibble

	lz2_token:
		ld   a, (hl)                ; read the command token
		inc  hl
		ld   (_g_LZSA2_Tok), a
		and  #0x18                  ; LL: literal-length field (bits 4-3)
		jr   Z, lz2_nolit           ; LL == 0 -> no literals
		rrca
		rrca
		rrca                        ; A = LL (1..3)
		cp   #3
		jr   NZ, lz2_litdirect      ; LL 1 or 2 -> count is direct
		call lz2_extlit             ; LL == 3 -> extended length into BC
		jr   lz2_litcopy
	lz2_litdirect:
		ld   c, a
		ld   b, #0
	lz2_litcopy:
		ldir                        ; emit literals straight from the stream

	lz2_nolit:
		ld   a, (_g_LZSA2_Tok)      ; decode the match offset from XYZ (bits 7-5)
		or   a, a
		jp   P, lz2_off0xx          ; bit7 = 0  -> 00x / 01x
		cp   #0xC0
		jr   NC, lz2_off11x         ; bit6 = 1  -> 11x
		jp   lz2_case10x            ; "10x": 13-bit offset
	lz2_off11x:
		cp   #0xE0
		jr   NC, lz2_case111        ; "111": repeat previous offset
		; "110": 16-bit offset -- high byte then low byte
		ld   b, (hl)
		inc  hl
		ld   c, (hl)
		inc  hl
		jr   lz2_gotoff
	lz2_off0xx:
		cp   #0x40
		jr   C, lz2_case00x         ; "00x": 5-bit offset
		; "01x": 9-bit offset -- inverted Z is bit 8, a stream byte is bits 0-7,
		; bits 9-15 are 1 (so high byte = 0xFE with bit 8 = invZ).
		and  #0x20                  ; isolate Z
		ld   b, #0xFF               ; Z == 0 -> invZ = 1 -> high = 0xFF
		jr   Z, lz2_c01_lo
		ld   b, #0xFE               ; Z == 1 -> invZ = 0 -> high = 0xFE
	lz2_c01_lo:
		ld   c, (hl)
		inc  hl
		jr   lz2_gotoff

	lz2_case00x:
		; 5-bit: nibble -> offset bits 1-4, invZ -> bit 0, bits 5-15 = 1.
		push af                     ; keep token for the Z bit
		call lz2_nibble             ; A = nibble (0..15)
		add  a, a                   ; nibble into bits 1-4
		or   #0xE0                  ; set bits 5-7
		ld   c, a
		ld   b, #0xFF               ; bits 8-15 = 1
		pop  af
		and  #0x20                  ; Z
		jr   NZ, lz2_gotoff         ; Z == 1 -> invZ = 0 -> bit0 = 0
		inc  c                      ; Z == 0 -> invZ = 1 -> bit0 = 1
		jr   lz2_gotoff

	lz2_case10x:
		; 13-bit: nibble -> offset bits 9-12, invZ -> bit 8, byte -> bits 0-7,
		; bits 13-15 = 1, then subtract 512.
		push af
		call lz2_nibble             ; A = nibble (bits 9-12)
		add  a, a                   ; into high-byte bits 1-4
		or   #0xE0                  ; high-byte bits 5-7 = 1 (offset bits 13-15)
		ld   b, a
		pop  af
		and  #0x20                  ; Z
		jr   NZ, lz2_c10_lo         ; Z == 1 -> invZ = 0 -> bit8 = 0
		inc  b                      ; Z == 0 -> invZ = 1 -> bit8 = 1
	lz2_c10_lo:
		ld   c, (hl)                ; offset bits 0-7
		inc  hl
		ld   a, b                   ; subtract 512 (0x200) from the 16-bit offset
		sub  a, #2
		ld   b, a
		jr   lz2_gotoff

	lz2_case111:
		; repeat offset: g_LZSA2_Off already holds it -- go straight to length.
		jr   lz2_matchlen

	lz2_gotoff:
		ld   (_g_LZSA2_Off), bc     ; remember this offset for a later "111"

	lz2_matchlen:
		ld   a, (_g_LZSA2_Tok)      ; MMM: match-length field (bits 2-0)
		and  #0x07
		cp   #7
		jr   Z, lz2_extmatch        ; MMM == 7 -> extended length
		add  a, #2                  ; else length = MMM + minmatch(2)
		ld   c, a
		ld   b, #0
		jr   lz2_copy
	lz2_extmatch:
		call lz2_matchext           ; BC = length, carry set = end of data
		jr   C, lz2_done

	lz2_copy:
		push hl                     ; park the source pointer
		ld   hl, (_g_LZSA2_Off)     ; negative offset
		add  hl, de                 ; HL = dst - distance (the back-reference)
		ldir                        ; copy the match (overlaps handled by LDIR)
		pop  hl
		jp   lz2_token

	lz2_done:
		ret

	;-------------------------------------------------------------------------
	; Read one nibble into A (0..15). Nibbles are packed high-then-low within a
	; stream byte; the spare low nibble persists across commands. Preserves BC/DE.
	lz2_nibble:
		ld   a, (_g_LZSA2_Nib)
		or   a, a
		jp   M, lz2_nib_pending     ; bit7 set -> use the pending low nibble
		ld   a, (hl)                ; none pending: consume a fresh byte
		inc  hl
		push af
		and  #0x0F                  ; stash its low nibble, flag ready
		or   #0x80
		ld   (_g_LZSA2_Nib), a
		pop  af
		rrca
		rrca
		rrca
		rrca                        ; return the high nibble now
		and  #0x0F
		ret
	lz2_nib_pending:
		and  #0x0F                  ; return the stashed low nibble
		push af
		xor  a, a
		ld   (_g_LZSA2_Nib), a      ; mark reservoir empty
		pop  af
		ret

	;-------------------------------------------------------------------------
	; Extended literal length -> BC. Nibble 0-14: 3 + nibble. Nibble 15: a byte
	; follows; 0-237 -> 18 + byte, 239.. -> a 16-bit little-endian length.
	lz2_extlit:
		call lz2_nibble
		cp   #15
		jr   Z, lz2_extlit_big
		add  a, #3
		ld   c, a
		ld   b, #0
		ret
	lz2_extlit_big:
		ld   a, (hl)
		inc  hl
		cp   #238
		jr   NC, lz2_len16          ; >= 238 -> 16-bit (238 itself is never emitted)
		add  a, #18
		ld   c, a
		ld   b, #0
		ret

	;-------------------------------------------------------------------------
	; Extended match length -> BC, carry set on EOD. Nibble 0-14: 9 + nibble.
	; Nibble 15: a byte follows; 0-231 -> 24 + byte, 232 -> end of data,
	; 233.. -> a 16-bit little-endian length.
	lz2_matchext:
		call lz2_nibble
		cp   #15
		jr   Z, lz2_matchext_big
		add  a, #9
		ld   c, a
		ld   b, #0
		or   a, a                   ; clear carry (not EOD)
		ret
	lz2_matchext_big:
		ld   a, (hl)
		inc  hl
		cp   #232
		jr   C, lz2_matchext_byte   ; < 232 -> 24 + byte
		jr   Z, lz2_eod             ; == 232 -> end of data
		jr   lz2_len16              ; >= 233 -> 16-bit
	lz2_matchext_byte:
		add  a, #24
		ld   c, a
		ld   b, #0
		or   a, a
		ret
	lz2_eod:
		scf                         ; signal end of data to the caller
		ret

	;-------------------------------------------------------------------------
	; Read a 16-bit little-endian length -> BC, clear carry.
	lz2_len16:
		ld   c, (hl)
		inc  hl
		ld   b, (hl)
		inc  hl
		or   a, a                   ; NC: not EOD
		ret
	__endasm;
}
