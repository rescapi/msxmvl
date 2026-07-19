#!/usr/bin/env bash
# Run every documentation example and assert its sentinel. This is the guarantee behind the
# documentation: no example is shown in the documentation unless it passes here on real
# (emulated) MSX2 hardware. Keep in sync with the examples referenced by the pages.
set -uo pipefail
H="$(cd "$(dirname "$0")" && pwd)"
fail=0
# Optional sharding for CI: set SHARD_TOTAL>0 and SHARD_INDEX in [0,SHARD_TOTAL) to run
# only this shard's slice of the examples (round-robin by call order). Unset = run all.
SHARD_TOTAL="${SHARD_TOTAL:-0}"; SHARD_INDEX="${SHARD_INDEX:-0}"; __i=0
run() {
	if [ "$SHARD_TOTAL" -gt 0 ] && [ $((__i % SHARD_TOTAL)) -ne "$SHARD_INDEX" ]; then
		__i=$((__i + 1)); return
	fi
	__i=$((__i + 1))
	if ! "$H/run_example.sh" "$@"; then fail=1; fi
}

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

# Decompression (unzx0) — ZX0: byte-exact oracle round-trip over a reference-packed
# corpus (edge cases + a 4 KB block), then a VRAM-target unpack read back from VRAM
run "$H/unzx0_01_roundtrip.c" "unzx0.c" a5 8
run "$H/unzx0_02_vram.c"      "unzx0.c" a5 8
run "$H/unzx7_01_roundtrip.c"     "unzx7.c" a5 8
run "$H/unpletter_01_roundtrip.c" "unpletter.c" a5 8
run "$H/unlzsa2_01_roundtrip.c"   "unlzsa2.c" a5 8

# State machine (fsm)
run "$H/fsm_01_states.c"      "fsm.c" a5 8
run "$H/game_menu_01.c"   "game_menu.c input.c" a5 8
run "$H/game_seq_01.c"    "game_seq.c" a5 8
run "$H/game_state_01.c"  "game_state.c" a5 8

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

# Sound (MSX-MUSIC) — OPLL is WRITE-ONLY, so assert the emulated chip's registers.
# The FMPAC extension needs the FMPAC BIOS ROM, which is COPYRIGHTED — present on real hardware
# and a configured local openMSX, but never on a bare C-BIOS CI runner (where it would hang on
# the missing chip). CI runners set CI=true; skip it there. Set MSXMVL_SKIP_FMPAC=1 to skip
# manually. Locally (ROM present) it runs and is asserted like the others.
if [ -n "${CI:-}" ] || [ -n "${MSXMVL_SKIP_FMPAC:-}" ]; then
  echo "  msxmusic_01_note.c: SKIP (FMPAC BIOS ROM is copyrighted; not available in CI — source verified locally)"
else
  EX_EXT=fmpac EX_DBG="Panasoft SW-M004 FMPAC regs" EX_DBGOFS=0x30 EX_DBGLEN=1 EX_DBGWANT=50 \
    run "$H/msxmusic_01_note.c" "bios.c system.c msx-music.c" a5 8
fi

# Sound (MSX-AUDIO) — the Y8950 does read back, so it self-verifies
EX_EXT=audio run "$H/msxaudio_01_reg.c" "msx-audio.c" a5 8

# MSX-AUDIO ADPCM — sample upload/readback, verified against the cartridge sample RAM
EX_EXT=audio EX_DBG="Generic MSX-Audio RAM" EX_DBGOFS=0x1000 EX_DBGLEN=8 EX_DBGWANT=1122334455667788 \
  run "$H/msxaudio_02_adpcm.c" "msx-audio.c" a5 8

# Sound (MoonBlaster) — the full MoonBlaster 1.4 replayer: sample-kit upload verified
# by readback, deterministic MoonBlaster_Tick fast-forward, then real BIOS-interrupt
# playback through H.TIMI. EX_DATALOC clears the replayer's home at 0xB000-0xC50F.
EX_EXT=audio EX_DATALOC=0xC600 run "$H/moonblaster_01_play.c" "moonblaster.c msx-audio.c" a5 8

# PSG sound effects (ayfx) -- priority SFX mixed into the shared PSG shadow
run "$H/ayfx_01.c"        "psg.c ayfx.c" a5 8

# ROM formats — 32 KB (data in page 2) and 48 KB (page 0 ours, BIOS gone)
EX_ROM=32 run "$H/rom32_01_bigrom.c" "" a5 8
EX_ROM=48 run "$H/rom48_01_page0.c" "" a5 8

# Interrupts — IM 2 takeover: handler-driven counting, then a clean uninstall
run "$H/isr_01_im2.c" "isr.c" a5 8

# Real-time clock (clock) — RP-5C01, MSX2+; CMOS save needs no disk
run "$H/clock_01_datetime.c"  "clock.c" a5 4
run "$H/clock_02_savedata.c"  "clock.c" a5 4

# Game saves (sram + pac) — battery-backed PAC cartridge SRAM. The openMSX `pac`
# extension is the SRAM-only Panasoft SW-M001: NO copyrighted ROM involved, so
# unlike the FMPAC these run everywhere, including CI. (The mapper-SRAM backend
# needs a bankpack MegaROM and is covered by test/banksram/run.sh.)
EX_EXT=pac run "$H/sram_01_pac.c" "sram.c" a5 8
EX_EXT=pac EX_DEFS='-DAPPSIGN="MVZ8"' run "$H/pac_01_compat.c" "pac.c sram.c" a5 8

# File I/O (dos) is NOT run here: it needs an MSX-DOS system disk + disk-ROM machine,
# which are copyrighted and cannot ship. Run it yourself with:
#   ./run_example_dos.sh dos_01_file.c "dos.c" a5 8            # DOS 2 handles
#   EX_DOS=1 ./run_example_dos.sh dos_02_fcb.c "dos.c" a5 8    # DOS 1 FCB
#   ./run_example_dos.sh disk_01_sector.c "disk.c" a5 8        # raw sectors (read)
#   ./run_example_dos.sh disk_02_write.c  "disk.c" a5 8        # raw sectors (write)
#   EX_EXT=moonsound ./run_example_dos.sh moonblaster_wave_01.c "moonblaster_wave.c" a5 8  # MoonSound Wave (needs OPL4 YRW801 ROM)

echo
# drift guard: the code shown in the documentation must be the code we just tested.
# Skip it when sharded (a partial run) — CI runs it once in its own step.
if [ "$SHARD_TOTAL" -eq 0 ]; then
	if ! bash "$H/verify_docs.sh"; then fail=1; fi
fi

echo
[ $fail -eq 0 ] && echo "ALL DOC EXAMPLES PASS" || echo "SOME DOC EXAMPLES FAILED"
exit $fail
