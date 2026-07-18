#!/usr/bin/env bash
# Run the MSX1-capable subset of the documentation examples on C-BIOS_MSX1.
#
# This is the evidence behind the "works on MSX1 too" claim: every example listed
# here is compiled with MSX_VERSION=MSX_1 and booted on an emulated MSX1.
#
# NOT here, and why (these are MSX2/V9938 features, and we do not pretend otherwise):
#   display        - page flipping is a V9938 feature
#   memseg/farmem  - the MSX2 RAM memory mapper does not exist on MSX1
#   draw/tile      - the V9938 command engine does not exist on MSX1
#   print (bitmap) - GRAPHIC4 does not exist on MSX1
#   scroll         - V9938 hardware scroll registers
#
# Beware two VACUOUS passes if you ever add them here: memseg_01 "passes" on MSX1
# because MemSeg_GetWindow reads the software shadow (which happily reports a
# mapper that isn't there), and display_01/02 "pass" because they only compute
# page numbers. Neither proves anything on an MSX1.
set -uo pipefail
H="$(cd "$(dirname "$0")" && pwd)"
export EX_MACHINE=C-BIOS_MSX1
fail=0
run() { if ! "$H/run_example.sh" "$@"; then fail=1; fi; }

# 3D math, fixed-point, integer helpers — pure computation
run "$H/g3d_01_mul.c"         "g3d.c"
run "$H/g3d_02_rotate.c"      "g3d.c"
run "$H/fixed_01_qmn.c"       "fixed_point.c" a5 4
run "$H/math_01_divmod.c"     "math.c" a5 4
run "$H/math_02_flip.c"       "math.c" a5 4
run "$H/math_03_random.c"     "math.c" a5 4
run "$H/string_01_fromuint.c" "string.c" a5 8

# RAM block ops
run "$H/memory_01_copyset.c"  "memory.c" a5 8
run "$H/memory_02_fast.c"     "memory.c" a5 8

# VDP — 16 KB VRAM, TMS9918 timing (>=29 T between accesses)
run "$H/vdp_01_fillpeek.c"    "vdp.c" a5 4
run "$H/vdp_02_pokepeek.c"    "vdp.c" a5 4
run "$H/vdp_03_writeread.c"   "vdp.c" a5 8
EX_VREG_ASSERT="7=0a" run "$H/vdp_04_regwrite.c" "vdp.c" a5 2

# VBlank — the TMS9918 raises the frame interrupt just like the V9938
run "$H/vsync_01_wait.c"      "vsync.c" a5 2

# Keyboard — same key matrix on MSX1
EX_KEYDOWN="8 0x01" run "$H/input_01_keydown.c" "input.c" a5 2
EX_KEYDOWN="8 0x01" run "$H/input_02_pushed.c"  "input.c" a5 2

# PSG — the AY-3-8910 is present on every MSX
run "$H/psg_01_tone.c"        "psg.c" a5 8

# Sprite pattern transforms — pure RAM
run "$H/sprfx_01_flip.c"      "sprite_fx.c" a5 8

# Logic modules — no MSX2 hardware at all
run "$H/compress_01_rlep.c"   "memory.c compress.c" a5 8
run "$H/fsm_01_states.c"      "fsm.c" a5 8
run "$H/mutex_01_locks.c"     "" a5 8
run "$H/localize_01_lang.c"   "localize.c" a5 8
run "$H/crypt_01_roundtrip.c" "crypt.c" a5 8

# Sound cartridges work on an MSX1 too (they carry their own chips). SCC (Konami) and MSX-Audio
# (Y8950) need no copyrighted BIOS, so they run everywhere including CI — verified on C-BIOS_MSX1.
EX_EXT=scc run "$H/scc_01_wave.c"   "bios.c scc.c" a5 8
EX_EXT=scc EX_DEFS="-DSCC_SLOT_MODE=SCC_SLOT_AUTO" run "$H/scc_02_detect.c" "bios.c scc.c" a5 8
EX_EXT=audio run "$H/msxaudio_01_reg.c" "msx-audio.c" a5 8
EX_EXT=audio EX_DBG="Generic MSX-Audio RAM" EX_DBGOFS=0x1000 EX_DBGLEN=8 EX_DBGWANT=1122334455667788 \
  run "$H/msxaudio_02_adpcm.c" "msx-audio.c" a5 8

# MSX-MUSIC: the FMPAC is a CARTRIDGE (its own OPLL + BIOS), so it works on an MSX1 just as on an
# MSX2. Verified: msxmusic_01_note PASSes on C-BIOS_MSX1 + fmpac. Skipped on CI (the FMPAC BIOS
# ROM is copyrighted); runs locally where the ROM is present.
if [ -n "${CI:-}" ] || [ -n "${MSXMVL_SKIP_FMPAC:-}" ]; then
  echo "  msxmusic_01_note.c: SKIP (FMPAC BIOS ROM is copyrighted; not available in CI)"
else
  EX_EXT=fmpac EX_DBG="Panasoft SW-M004 FMPAC regs" EX_DBGOFS=0x30 EX_DBGLEN=1 EX_DBGWANT=50 \
    run "$H/msxmusic_01_note.c" "bios.c system.c msx-music.c" a5 8
fi

# ROM formats + interrupts — slot logic and IM 2 are generation-independent,
# and these assert real content (checksums, page-0 bytes, handler-driven ticks)
EX_ROM=32 run "$H/rom32_01_bigrom.c" "" a5 8
EX_ROM=48 run "$H/rom48_01_page0.c" "" a5 8
run "$H/isr_01_im2.c" "isr.c" a5 8

echo
[ $fail -eq 0 ] && echo "ALL MSX1 EXAMPLES PASS" || echo "SOME MSX1 EXAMPLES FAILED"
exit $fail
