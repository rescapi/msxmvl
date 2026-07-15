/* msxmvl — fast MSX2 primitives (M1-optimized). C API, compile with SDCC __sdcccall(1). */
#ifndef MSXMVL_H
#define MSXMVL_H
typedef unsigned char u8;
typedef unsigned int  u16;

/* --- VRAM (V9938) --- addr/transfer are split so the transfer loops take <=2 reg args. */
void mvc_vram_set_write(u16 addr) __naked;              /* set VRAM write address (<16K) */
void mvc_vram_set_read(u16 addr)  __naked;              /* set VRAM read address */
void mvc_vram_write(const u8* src, u16 count) __naked;  /* RAM->VRAM (OUTI; safe all modes) */
void mvc_vram_read(u8* dst, u16 count) __naked;         /* VRAM->RAM (INI; safe all modes) */
void mvc_vram_fill_fast(u8 val, u16 count) __naked;     /* VRAM fill; BLANKED-ONLY (12T<15T) */

/* --- RAM --- */
void mvc_memset_fast(u8* dst, u16 count_val);           /* push-based; see note (needs even count) */

#endif

/* safe (all-display-mode) VRAM fill: OUTI from a 1-byte source held in a reg pair.
   For strict MSXgl parity the compat shim uses this, not the blanked-only fast fill. */
void mvc_vram_fill_safe(u8 val, u16 count) __naked;
