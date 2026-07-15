#!/usr/bin/env bash
# Run every documentation example and assert its sentinel. This is the guarantee behind the
# documentation: no example is shown in the documentation unless it passes here on real
# (emulated) MSX2 hardware. Keep in sync with the examples referenced by the pages.
set -uo pipefail
H="$(cd "$(dirname "$0")" && pwd)"
fail=0
run() { if ! "$H/run_example.sh" "$@"; then fail=1; fi; }

# 3D math (g3d)
run "$H/g3d_01_mul.c"        "g3d.c"
run "$H/g3d_02_rotate.c"     "g3d.c"

# Double buffering (display)
run "$H/display_01_pages.c"  "display.c"
run "$H/display_02_flip.c"   "display.c"
EX_VREG_ASSERT="2=7f" run "$H/display_03_showpage.c" "display.c" a5 2

# VBlank sync (vsync)
run "$H/vsync_01_wait.c"     "vsync.c" a5 2

# Segment windowing (memseg)
run "$H/memseg_01_setget.c"  "memseg.c" a5 4
run "$H/memseg_02_window.c"  "memseg.c" a5 4

# Far pointers (farmem)
run "$H/farmem_01_pokepeek.c"  "memseg.c farmem.c" a5 4
run "$H/farmem_02_readwrite.c" "memseg.c farmem.c" a5 8
run "$H/farmem_03_boundary.c"  "memseg.c farmem.c" a5 8
run "$H/farmem_04_with.c"      "memseg.c farmem.c" a5 4
run "$H/farmem_05_set.c"       "memseg.c farmem.c" a5 4

# VRAM + registers (vdp) — compat layer
run "$H/vdp_01_fillpeek.c"   "vdp.c" a5 4
run "$H/vdp_02_pokepeek.c"   "vdp.c" a5 4
run "$H/vdp_03_writeread.c"  "vdp.c" a5 8
EX_VREG_ASSERT="7=0a" run "$H/vdp_04_regwrite.c" "vdp.c" a5 2

# Keyboard (input) — compat layer; harness holds SPACE (matrix row 8, bit 0)
EX_KEYDOWN="8 0x01" run "$H/input_01_keydown.c" "input.c" a5 2
EX_KEYDOWN="8 0x01" run "$H/input_02_pushed.c"  "input.c" a5 2

# RAM block ops (memory)
run "$H/memory_01_copyset.c"  "memory.c" a5 8
run "$H/memory_02_fast.c"     "memory.c" a5 8
# the dynamic allocator: alloc/free/reuse/coalesce. Nothing else covered it.
run "$H/memory_03_dynamic.c"  "memory.c" a5 8

# Integer helpers (math)
run "$H/math_01_divmod.c"     "math.c" a5 4
run "$H/math_02_flip.c"       "math.c" a5 4
run "$H/math_03_random.c"     "math.c" a5 4

# Number -> text (string)
run "$H/string_01_fromuint.c" "string.c" a5 8

# Sound (psg)
run "$H/psg_01_tone.c"        "psg.c" a5 8

# Fixed-point (fixed_point)
run "$H/fixed_01_qmn.c"       "fixed_point.c" a5 4

# Bitmap drawing (draw) — GRAPHIC4 command engine
run "$H/draw_01_point.c"      "vdp.c draw.c" a5 4
run "$H/draw_02_shapes.c"     "vdp.c draw.c" a5 4

# Sprite pattern transforms (sprite_fx) — pure RAM
run "$H/sprfx_01_flip.c"      "sprite_fx.c" a5 8

# Text output (print) — bitmap font glyph pixels
run "$H/print_01_text.c"      "vdp.c vdp_inl.c print.c" a5 8

# Tilemaps (tile) — VRAM->VRAM tile copy (tile.c brings its own command fns)
run "$H/tile_01_map.c"        "vdp.c tile.c" a5 8

# Hardware scroll (scroll) — offset accumulation/clamp
run "$H/scroll_01_offset.c"   "vdp.c scroll.c" a5 8

# Decompression (compress) — RLEp; uses Mem_Copy/Mem_Set, so memory.c too
run "$H/compress_01_rlep.c"   "memory.c compress.c" a5 8

# State machine (fsm)
run "$H/fsm_01_states.c"      "fsm.c" a5 8

# Mutexes (mutex) — header-only inline: no link object
run "$H/mutex_01_locks.c"     "" a5 8

# Localization (localize)
run "$H/localize_01_lang.c"   "localize.c" a5 8

# Save-code obfuscation (crypt)
run "$H/crypt_01_roundtrip.c" "crypt.c" a5 8
# crypt is hand-written asm: also exercise zero bytes, key wrap, the failure paths and a
# caller-supplied map/code table. A 4-byte round trip does not touch any of those.
run "$H/crypt_02_verify.c"    "crypt.c" a5 8

# Sound (SCC) — Konami wavetable cartridge; also passes on an SCC+ (-ext scc+)
EX_EXT=scc run "$H/scc_01_wave.c"   "bios.c scc.c" a5 8
EX_EXT=scc EX_DEFS="-DSCC_SLOT_MODE=SCC_SLOT_AUTO" run "$H/scc_02_detect.c" "bios.c scc.c" a5 8

# Sound (MSX-MUSIC) — OPLL is WRITE-ONLY, so assert the emulated chip's registers
EX_EXT=fmpac EX_DBG="Panasoft SW-M004 FMPAC regs" EX_DBGOFS=0x30 EX_DBGLEN=1 EX_DBGWANT=50 \
  run "$H/msxmusic_01_note.c" "bios.c system.c msx-music.c" a5 8

# Sound (MSX-AUDIO) — the Y8950 does read back, so it self-verifies
EX_EXT=audio run "$H/msxaudio_01_reg.c" "msx-audio.c" a5 8

# MSX-AUDIO ADPCM — sample upload/readback, verified against the cartridge sample RAM
EX_EXT=audio EX_DBG="Generic MSX-Audio RAM" EX_DBGOFS=0x1000 EX_DBGLEN=8 EX_DBGWANT=1122334455667788 \
  run "$H/msxaudio_02_adpcm.c" "msx-audio.c" a5 8

# Real-time clock (clock) — RP-5C01, MSX2+; CMOS save needs no disk
run "$H/clock_01_datetime.c"  "clock.c" a5 4
run "$H/clock_02_savedata.c"  "clock.c" a5 4

# File I/O (dos) is NOT run here: it needs an MSX-DOS system disk + disk-ROM machine,
# which are copyrighted and cannot ship. Run it yourself with:
#   ./run_example_dos.sh dos_01_file.c "dos.c" a5 8            # DOS 2 handles
#   EX_DOS=1 ./run_example_dos.sh dos_02_fcb.c "dos.c" a5 8    # DOS 1 FCB
#   ./run_example_dos.sh disk_01_sector.c "disk.c" a5 8        # raw sectors (read)
#   ./run_example_dos.sh disk_02_write.c  "disk.c" a5 8        # raw sectors (write)

echo
# drift guard: the code shown in the documentation must be the code we just tested
if ! bash "$H/verify_docs.sh"; then fail=1; fi

echo
[ $fail -eq 0 ] && echo "ALL DOC EXAMPLES PASS" || echo "SOME DOC EXAMPLES FAILED"
exit $fail
