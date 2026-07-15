#include "msxmvl.h"
/* args (SDCC __sdcccall(1)): 1st -> HL, 2nd -> DE. All __naked. */

void mvc_vram_set_write(u16 addr) __naked {
    (void)addr; __asm
        ld a,l
        out (0x99),a
        ld a,h
        or #0x40           ; write-enable bit
        out (0x99),a
        ret
    __endasm;
}
void mvc_vram_set_read(u16 addr) __naked {
    (void)addr; __asm
        ld a,l
        out (0x99),a
        ld a,h             ; read mode (no 0x40)
        out (0x99),a
        ret
    __endasm;
}
/* mvc_vram_write(src=HL, count=DE): OUTI, page+remainder, safe all display modes (18T/byte). */
void mvc_vram_write(const u8* src, u16 count) __naked {
    (void)src;(void)count; __asm
        ld c,#0x98
        ld a,d
        or a
        jr z, mvw_rem
    mvw_pg:
        ld b,#0
    mvw_p8:
        .rept 8
        outi
        .endm
        jr nz, mvw_p8
        dec a
        jr nz, mvw_pg
    mvw_rem:
        ld a,e
        or a
        ret z
        ld b,e
    mvw_r1:
        outi
        jr nz, mvw_r1
        ret
    __endasm;
}
/* mvc_vram_read(dst=HL, count=DE): INI, mirror of write. */
void mvc_vram_read(u8* dst, u16 count) __naked {
    (void)dst;(void)count; __asm
        ld c,#0x98
        ld a,d
        or a
        jr z, mvr_rem
    mvr_pg:
        ld b,#0
    mvr_p8:
        .rept 8
        ini
        .endm
        jr nz, mvr_p8
        dec a
        jr nz, mvr_pg
    mvr_rem:
        ld a,e
        or a
        ret z
        ld b,e
    mvr_r1:
        ini
        jr nz, mvr_r1
        ret
    __endasm;
}
/* mvc_vram_fill_fast(val=L, count=DE): out-based, BLANKED-ONLY. */
void mvc_vram_fill_fast(u8 val, u16 count) __naked {
    (void)val;(void)count; __asm
        ld c,a             ; val already in A (arg1 u8 -> A); save in C
        ld a,d
        or a
        jr z, mvf_rem
        ld a,c
    mvf_pg:
        ld b,#0
    mvf_p8:
        .rept 8
        out (0x98),a
        .endm
        djnz mvf_p8
        dec d
        jr nz, mvf_pg
    mvf_rem:
        ld a,e
        or a
        ret z
        ld b,e
        ld a,c
    mvf_r1:
        out (0x98),a
        djnz mvf_r1
        ret
    __endasm;
}

/* safe fill: write val via out (0x98) spaced >=15T using a per-byte loop that is >=15T.
   Simplest correct-everywhere: out(12T)+dec-pair as spacing. Here: 16x(out+nop)=17T/byte. */
void mvc_vram_fill_safe(u8 val, u16 count) __naked {
    (void)val;(void)count; __asm
        ld c,a          ; val already in A; save in C
        ld a,d
        or a
        jr z, mvfs_rem
        ld a,c
    mvfs_pg:
        ld b,#0
    mvfs_p:
        .rept 8
        out (0x98),a
        nop
        .endm
        djnz mvfs_p
        dec d
        jr nz, mvfs_pg
    mvfs_rem:
        ld a,e
        or a
        ret z
        ld b,e
        ld a,c
    mvfs_r:
        out (0x98),a
        nop
        djnz mvfs_r
        ret
    __endasm;
}
