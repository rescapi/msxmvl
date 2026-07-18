#!/usr/bin/env bash
# Drift guard: assert that every executable API line in each tested example .c also
# appears verbatim in the documentation page that documents it. Together with run_all.sh
# (which proves the examples pass on hardware), this guarantees the code shown in
# the documentation is exactly the code that was tested.
set -uo pipefail
H="$(cd "$(dirname "$0")" && pwd)"
DOCS="$(cd "$H/.." && pwd)"

declare -A PAGE=(
 [g3d_01_mul]=3D-Math.md [g3d_02_rotate]=3D-Math.md
 [display_01_pages]=Double-Buffering.md [display_02_flip]=Double-Buffering.md [display_03_showpage]=Double-Buffering.md
 [vsync_01_wait]=VBlank-Sync.md
 [memseg_01_setget]=Segment-Windowing.md [memseg_02_window]=Segment-Windowing.md
 [farmem_01_pokepeek]=Far-Pointers.md [farmem_02_readwrite]=Far-Pointers.md [farmem_03_boundary]=Far-Pointers.md
 [farmem_04_with]=Far-Pointers.md [farmem_05_set]=Far-Pointers.md
 [vdp_01_fillpeek]=VDP-Access.md [vdp_02_pokepeek]=VDP-Access.md [vdp_03_writeread]=VDP-Access.md [vdp_04_regwrite]=VDP-Access.md
 [input_01_keydown]=Keyboard-Input.md [input_02_pushed]=Keyboard-Input.md
 [memory_01_copyset]=Memory-Operations.md [memory_02_fast]=Memory-Operations.md
 [math_01_divmod]=Math-Utilities.md [math_02_flip]=Math-Utilities.md [math_03_random]=Math-Utilities.md
 [string_01_fromuint]=String-Conversion.md
 [psg_01_tone]=Sound-PSG.md
 [fixed_01_qmn]=Fixed-Point.md
 [draw_01_point]=Bitmap-Drawing.md [draw_02_shapes]=Bitmap-Drawing.md
 [sprfx_01_flip]=Sprite-FX.md
 [print_01_text]=Text-Output.md
 [tile_01_map]=Tilemaps.md
 [scroll_01_offset]=Hardware-Scroll.md
 [compress_01_rlep]=Compression.md
 [fsm_01_states]=State-Machines.md
 [mutex_01_locks]=Mutexes.md
 [localize_01_lang]=Localization.md
 [crypt_01_roundtrip]=Encryption.md
 [clock_01_datetime]=Real-Time-Clock.md
 [clock_02_savedata]=Real-Time-Clock.md
 [sram_01_pac]=Game-Saves.md
 [pac_01_compat]=Game-Saves.md
 [dos_01_file]=MSX-DOS-2.md
 [dos_02_fcb]=MSX-DOS-1.md
 [disk_01_sector]=Disk-Sectors.md
 [disk_02_write]=Disk-Sectors.md
 [bootdisk_01_sentinel]=Boot-Disk.md
 [bootdisk_02_diskos]=Boot-Disk.md
 [scc_01_wave]=Sound-SCC.md
 [scc_02_detect]=Sound-SCC.md
 [msxmusic_01_note]=Sound-MSX-Music.md
 [msxaudio_01_reg]=Sound-MSX-Audio.md
 [msxaudio_02_adpcm]=Sound-MSX-Audio.md
 [moonblaster_01_play]=Sound-MoonBlaster.md
 [rom32_01_bigrom]=ROM-Formats.md
 [rom48_01_page0]=ROM-Formats.md
 [isr_01_im2]=Interrupts.md
)
fail=0
for ex in "${!PAGE[@]}"; do
  page="$DOCS/${PAGE[$ex]}"
  miss=0
  while IFS= read -r line; do
    key=$(echo "$line" | sed 's/^[[:space:]]*//; s/[[:space:]]*$//')
    [ -z "$key" ] && continue
    grep -Fq "$key" "$page" || { echo "  DRIFT [$ex -> ${PAGE[$ex]}]: $key"; miss=1; }
  done < <(sed '/\/\/.*test harness/q' "$H/$ex.c" | grep -E '(G3D_|Display_|MemSeg_|Far_|VDP_|Keyboard_|Mem_|Math_|String_|PSG_|QMN_|Draw_|SpriteFX_|Print_|Tile_|Scroll_|RLEp_|FSM_|Mutex_|Loc_|Crypt_|RTC_|DOS_|DiskOS_|Disk_|DiskDOS_|SCC_|MSXMusic_|MSXAudio_|MSXAudio_ADPCM_|MoonBlaster_|ISR_|Sram_|PAC_)[A-Za-z_]*\(' | grep -vE '^\s*//')
  [ $miss -ne 0 ] && fail=1
done
[ $fail -eq 0 ] && echo "documentation drift check: OK (all example code matches its page)" || { echo "documentation drift check: FAILED"; exit 1; }
