#!/usr/bin/env bash
# bankpack -- turnkey MegaROM builder for banked code (ASCII-8/16, Konami,
# Konami-SCC mappers; images up to the mapper's 256-segment limit).
#
# Pure bash + SDCC toolchain + coreutils (no Python). Three modes:
#
#   bankpack.sh --gen   <manifest> [outdir]             generate far.h + far thunks
#   bankpack.sh         <manifest> <out.rom> [rom_kb]   link banks + assemble the ROM
#   bankpack.sh --build <manifest> <out.rom> [rom_kb]   ONE command: gen + compile +
#                                                       assemble + link + ROM
#
# The developer writes plain C and a manifest. A bank source marks a far-callable
# DEFINITION with FAR_FN (e.g. `FAR_FN u16 play_level(u16 level) {...}`); the gen
# step scans the bank sources for those (explicit FAR manifest lines also still
# work) and emits a header of call declarations (callers just `#include "far.h"`
# and call `play_level(...)` -- the old `far_<name>(...)` spelling remains as an
# alias) plus the resident thunks + dispatch table. The build mode links each bank
# at its window address, patches the dispatch table with the real function
# addresses, and assembles the ROM (rom_kb, default 64). `--build` chains the whole pipeline:
# gen, compile each OBJ's sibling .c/.asm, assemble farrt + thunks, then build.
#
# Manifest (whitespace; '#' comments; blank lines ignored):
#   SEG   RUN      OBJ [OBJ ...]      one bank per line; first line = RESIDENT
#   FAR   SEG      <C prototype>      declare a far-callable function in SEG
#     e.g.  FAR 5 u16 farA(u16 x)    (optional -- a bank source can mark the
#                                     definition itself with FAR_FN instead and
#                                     skip the manifest line; see scan_far_fn)
#   FARTAB base slot0 slot1 ...       (advanced/manual; auto-derived from FAR)
#   RAMBASE <addr>                    statics RAM base (default 0xC000)
#   RESERVE <lo> <hi>                 program-owned RAM [lo,hi) (e.g. __at data);
#                                     build FAILS if the layout or SHADOW touches it
#   SHADOW  <addr>                    memseg shadow byte (default 0xE020;
#                                     env BANKPACK_SHADOW overrides)
#   MAPPER  ASCII8|ASCII16|KONAMI|KONAMI_SCC
#                                     cartridge mapper (default ASCII8). Every
#                                     supported mapper selects the 0x8000 window
#                                     with ONE 8-bit register write, so farrt and
#                                     the generated thunks are identical for all
#                                     of them -- only the select register address,
#                                     the segment size (ASCII16: 16 KB, the rest:
#                                     8 KB) and per-mapper build checks differ
#                                     (see the mapper table in parse_manifest).
#                                     The final report line prints the matching
#                                     openMSX -romtype.
#   SRAM <kb> [select]                battery-backed SRAM chip on the cartridge
#                                     (mapper-SRAM: requires MAPPER ASCII8 or
#                                     ASCII16). Emits SRAM_SELECT / SRAM_SIZE /
#                                     SRAM_SHADOW / SRAM_BANKREG into far.h for
#                                     lib/ext/sram.c's SRAM_BACKEND_MAPPER. The
#                                     select value that maps SRAM instead of ROM
#                                     into the window is rom_kb/8 on ASCII8 (the
#                                     first bit above the ROM's banks; [select]
#                                     overrides it for Wizardry-style fixed
#                                     wiring) and 0x10 on ASCII16. Sizes: ASCII8
#                                     2/8/32 KB, ASCII16 2/8 KB (the openMSX
#                                     ASCII8SRAM*/ASCII16SRAM* -romtypes). The
#                                     .rom image itself is unchanged -- SRAM is
#                                     a chip on the cart, not file content. A
#                                     select value that collides with a code
#                                     segment number is a hard build error (its
#                                     thunk would map SRAM, not code).
#
# C-RUNTIME DATA MODEL (docs/Code-Banking-Data-Model.md): every bank may have
# writable C statics. The resident links its _DATA/_INITIALIZED at RAMBASE; each
# later bank gets an explicit -b _DATA placing its statics in the next DISJOINT
# RAM slice. The per-bank init table (RAM dest, length, in-window ROM source) and
# the BSS union are PATCHED into farrt.asm's _bp_datatab, which farrt walks
# before _main: zero the union, copy the resident initializers, then map each
# bank and copy its slice.
#
# Far-call convention (SDCC --sdcccall 1): args must be REGISTER-passed
# (16-bit/pointer in HL/DE/BC) and returns 16-bit/pointer/void (HL/DE). The thunk
# preserves those plus IX; it clobbers only A/F. Avoid u8/bool/char args/returns
# (they ride in A) and >~3 args (spill to the stack) -- use u16.
set -uo pipefail

WINDOW=0x8000            # page-2 window base: banked code runs here, resident below
SYSTEM_RAM=0xF380        # BIOS work area: statics must stay below this
DATATAB_MAX=32           # capacity of farrt.asm's _bp_datatab (entries); raising it
                         # must grow _bp_datatab's .ds in farrt.asm in lock-step
# SHADOW (below) and ADDR_BANK2 (per-mapper, set by parse_manifest's mapper
# table) are single-sourced in THIS SCRIPT and injected into farrt.asm and the
# generated thunks at LINK time (-g), so the .asm files cannot drift.

die() { echo "bankpack: $*" >&2; exit 1; }

# Locate SDCC's z80 runtime library (for *, /, long, float helpers). Override
# with $BANKPACK_Z80LIB. Empty if not found -> resident helper pool is skipped
# (fine for banks that use no such operations).
find_z80lib() {
  local c
  for c in "${BANKPACK_Z80LIB:-}" \
           /usr/share/sdcc/lib/z80/z80.lib \
           /usr/lib/sdcc/lib/z80/z80.lib \
           /usr/local/share/sdcc/lib/z80/z80.lib; do
    [ -n "$c" ] && [ -f "$c" ] && { echo "$c"; return; }
  done
}

# ---- shared manifest parse: fills SEGS/RUNS/OBJS, FAR_SEG/FAR_NAME/FAR_PROTO, FARTAB ----
parse_manifest() {
  SEGS=(); RUNS=(); OBJS=(); FAR_SEG=(); FAR_NAME=(); FAR_PROTO=(); FARTAB=""
  RAMBASE=""; RESERVES=(); MSHADOW=""; MAPPER=""; SRAM_KB=""; SRAM_SEL=""
  local a b rest s
  while read -r a b rest; do
    [ -z "${a:-}" ] && continue
    case "$a" in \#*) continue;; esac
    case "$a" in
      FAR)
        # b = segment, rest = C prototype; name = identifier just before '('
        local name; name=$(printf '%s' "$rest" | sed 's/(.*//' | awk '{print $NF}')
        [ -n "$name" ] || die "cannot parse function name from: FAR $b $rest"
        FAR_SEG+=("$b"); FAR_NAME+=("$name"); FAR_PROTO+=("$rest") ;;
      FARTAB)
        FARTAB="$b $rest" ;;
      RAMBASE)
        [ -n "$b" ] || die "RAMBASE needs an address"
        RAMBASE="$b" ;;
      RESERVE)
        [ -n "$b" ] && [ -n "$rest" ] || die "RESERVE needs <lo> <hi>"
        RESERVES+=("$b ${rest%% *}") ;;
      SHADOW)
        [ -n "$b" ] || die "SHADOW needs an address"
        MSHADOW="$b" ;;
      MAPPER)
        [ -n "$b" ] || die "MAPPER needs a type (ASCII8, ASCII16, KONAMI, KONAMI_SCC)"
        MAPPER="$b" ;;
      SRAM)
        [ -n "$b" ] || die "SRAM needs a size in KB (ASCII8: 2/8/32; ASCII16: 2/8)"
        SRAM_KB="$b"; SRAM_SEL="${rest%% *}" ;;
      *)
        [ -n "$b" ] && [ -n "$rest" ] || die "bad manifest line: $a $b $rest"
        SEGS+=("$a"); RUNS+=("$b"); OBJS+=("$rest") ;;
    esac
  done < <(sed 's/#.*//' "$1")
  RAMBASE="${RAMBASE:-0xC000}"
  SHADOW="${BANKPACK_SHADOW:-${MSHADOW:-0xE020}}"
  # Mapper table: the ONE register that selects the 0x8000 window (ADDR_BANK2,
  # a link-time -g symbol; the 8-bit `ld (ADDR_BANK2),a` in farrt and the
  # thunks serves every mapper here), the segment size, and the matching
  # openMSX -romtype. The KONAMI/KONAMI_SCC select registers sit INSIDE the
  # window: harmless -- the mapper eats the write, ROM reads are unaffected,
  # and farrt/thunks only ever write it.
  case "${MAPPER:-ASCII8}" in
    ASCII8)     SEGSIZE=8192;  ADDR_BANK2=0x7000; ROMTYPE=ASCII8 ;;
    ASCII16)    SEGSIZE=16384; ADDR_BANK2=0x7000; ROMTYPE=ASCII16 ;;
    KONAMI)     SEGSIZE=8192;  ADDR_BANK2=0x8000; ROMTYPE=Konami ;;
    KONAMI_SCC) SEGSIZE=8192;  ADDR_BANK2=0x9000; ROMTYPE=KonamiSCC ;;
    *) die "unknown MAPPER '${MAPPER}' (supported: ASCII8, ASCII16, KONAMI, KONAMI_SCC)" ;;
  esac
  # Mapper-wide manifest checks (here, not in the build step, so --gen fails
  # just as early):
  # - the RESIDENT must be SEG 0: every mapper's reset state maps segment 0 at
  #   0x4000, and on KONAMI it is hardware-FIXED there (no register exists).
  # - every SEG must fit the mappers' 8-bit select register.
  # - KONAMI_SCC quirk: a select value whose low 6 bits are all 1 (0x3F, 0x7F,
  #   0xBF, 0xFF) maps the SCC SOUND CHIP at 0x9800-0x9FFF instead of ROM -- a
  #   thunk mapping such a segment would call into SCC registers, not code.
  if [ "${#SEGS[@]}" -ge 1 ] && [ "$((SEGS[0]))" -ne 0 ]; then
    die "resident bank must be SEG 0 (the mappers' reset state; hardware-fixed on KONAMI), got SEG ${SEGS[0]}"
  fi
  for s in "${SEGS[@]:-}"; do
    [ -n "$s" ] || continue
    [ "$((s))" -le 255 ] || die "SEG $s does not fit the mapper's 8-bit select register (max 255)"
    if [ "${MAPPER:-ASCII8}" = "KONAMI_SCC" ] && [ $(( s & 0x3F )) -eq "$((0x3F))" ]; then
      die "SEG $s would map the SCC sound chip, not ROM (on KONAMI_SCC a select value with the low 6 bits all 1 -- 0x3F/0x7F/0xBF/0xFF -- enables the SCC at 0x9800); move this code bank"
    fi
  done
}

# ---- SRAM line: validate + derive the select value (needs rom_kb) ----
# Sets SRAM_SELECT (the bank-select value that maps SRAM instead of ROM into
# the 0x8000 window) and SRAM_ROMTYPE (the matching openMSX -romtype). Hardware
# ground truth (docs/SRAM.md, openMSX device sources): on ASCII8 the SRAM
# enable bit is the ROM size in 8 KB banks -- the first bit ABOVE the ROM's
# bank numbers (overridable for Wizardry-style fixed wiring); on ASCII16 it is
# exactly 0x10 (no block bits; the chip mirrors through the 16 KB window).
# Guard: a code SEG sharing bits with the select value is a hard error -- the
# thunk writing that segment number would map SRAM, not code.
sram_config() {  # <rom_kb>
  SRAM_SELECT=""; SRAM_ROMTYPE=""
  [ -n "$SRAM_KB" ] || return 0
  case "${MAPPER:-ASCII8}" in
    ASCII8)
      case "$SRAM_KB" in 2|8|32) ;; *) die "SRAM $SRAM_KB KB: ASCII8 mapper-SRAM comes in 2, 8 or 32 KB" ;; esac
      SRAM_SELECT="${SRAM_SEL:-$(( $1 / 8 ))}" ;;
    ASCII16)
      case "$SRAM_KB" in 2|8) ;; *) die "SRAM $SRAM_KB KB: ASCII16 mapper-SRAM comes in 2 or 8 KB" ;; esac
      SRAM_SELECT="${SRAM_SEL:-16}" ;;
    *) die "SRAM needs MAPPER ASCII8 or ASCII16 (the SRAM chip hangs off the ASCII mappers' select registers), got ${MAPPER}" ;;
  esac
  SRAM_ROMTYPE="${MAPPER:-ASCII8}SRAM$SRAM_KB"
  local s
  for s in "${SEGS[@]:-}"; do
    [ -n "$s" ] || continue
    if [ $(( s & SRAM_SELECT )) -ne 0 ]; then
      die "SRAM select value $(printf 0x%02x "$((SRAM_SELECT))") collides with code SEG $s (writing that segment's select maps SRAM, not ROM -- shrink the ROM or move the bank)"
    fi
  done
}

# ---- FAR_FN scan: derive FAR entries from annotated bank sources ----
# A bank .c file may mark a far-callable DEFINITION on one line:
#     FAR_FN u16 play_level(u16 level) { ... }
# far.h #defines FAR_FN to nothing, so the annotation costs the compiler
# nothing -- this scan is what reads it. Pure text, no compile step (same style
# as the FAR prototype parsing): for every OBJ of every bank, map <name>.rel to
# <name>.c NEXT TO THE MANIFEST; lines whose FIRST token is FAR_FN contribute a
# far entry -- prototype = the rest of the line up to '{' (so the full prototype
# must sit on the FAR_FN line), owning SEG = the bank whose OBJ list named the
# .rel. Explicit FAR lines keep working and are appended first (parse order), so
# --gen and the build derive the SAME slot order. The same function arriving via
# both routes (or twice) is an error; FAR_FN in a RESIDENT source is an error
# (resident code is always mapped -- it needs no thunk).
scan_far_fn() {  # <manifest>
  local mdir; mdir=$(dirname "$1")
  local i obj src line proto name j
  for i in "${!SEGS[@]}"; do
    for obj in ${OBJS[$i]}; do
      case "$obj" in *.rel) src="$mdir/${obj%.rel}.c" ;; *) continue ;; esac
      [ -f "$src" ] || continue
      while IFS= read -r line; do
        proto=$(printf '%s' "$line" | sed -e 's/^[[:space:]]*FAR_FN[[:space:]]*//' -e 's/{.*//' -e 's/[[:space:]]*$//')
        case "$proto" in
          *\(*\)*) ;;
          *) die "FAR_FN needs the full prototype on one line: $src: $line" ;;
        esac
        name=$(printf '%s' "$proto" | sed 's/(.*//' | awk '{print $NF}')
        [ -n "$name" ] || die "cannot parse function name from FAR_FN line: $src: $line"
        [ "$i" -ne 0 ] || die "FAR_FN on '$name' in resident source $src -- resident functions need no FAR_FN"
        for j in "${!FAR_NAME[@]}"; do
          [ "${FAR_NAME[$j]}" != "$name" ] \
            || die "far function '$name' declared twice (FAR_FN in $src duplicates an earlier FAR/FAR_FN entry)"
        done
        FAR_SEG+=("${SEGS[$i]}"); FAR_NAME+=("$name"); FAR_PROTO+=("$proto")
      done < <(awk '$1=="FAR_FN"' "$src")
    done
  done
}

# ---- shared emitter: far.h + _bp_far_thunks.asm from the FAR entries ----
emit_far_files() {  # <outdir>
  local outdir="$1" H T i n seg slot
  H="$outdir/far.h"; T="$outdir/_bp_far_thunks.asm"
  {
    echo "// generated by 'bankpack --gen' -- do not edit"
    echo "#ifndef BANKPACK_FAR_H"
    echo "#define BANKPACK_FAR_H"
    echo '#include "types.h"'
    echo "// FAR_FN marks a far-callable DEFINITION in a bank source. It expands to"
    echo "// nothing: bankpack's text scan reads the annotation, the compiler never does."
    echo "#define FAR_FN"
    echo "// One pair per far function: the natural name (the thunk carries the"
    echo "// function's own name, so callers just write name(...)) and the far_"
    echo "// spelling (alias label at the same address, for existing callers)."
    for i in "${!FAR_NAME[@]}"; do
      printf 'extern %s;\n' "${FAR_PROTO[$i]}"
      # the same prototype with the function name prefixed far_
      printf 'extern %s;\n' \
        "$(printf '%s' "${FAR_PROTO[$i]}" | sed 's/\([A-Za-z_][A-Za-z0-9_]*\)[[:space:]]*(/far_\1(/')"
    done
    if [ -n "${SRAM_SELECT:-}" ]; then
      echo "// SRAM (manifest 'SRAM $SRAM_KB'): constants for lib/ext/sram.c's"
      echo "// SRAM_BACKEND_MAPPER. Single-sourced from the manifest + mapper table."
      printf '#define SRAM_SELECT  0x%02x    /* select value that maps SRAM into the window */\n' "$((SRAM_SELECT))"
      printf '#define SRAM_SIZE    %uu   /* usable SRAM bytes */\n' "$((SRAM_KB * 1024))"
      printf '#define SRAM_SHADOW  0x%04x  /* window shadow byte (segment now mapped) */\n' "$((SHADOW))"
      printf '#define SRAM_BANKREG 0x%04x  /* the 0x8000 window'"'"'s bank-select register */\n' "$((ADDR_BANK2))"
    fi
    echo "#endif"
  } > "$H"

  if [ "${#FAR_NAME[@]}" -eq 0 ]; then
    echo "bankpack: gen: wrote $H (no far functions; SRAM constants only)"
    return 0
  fi

  {
    echo ";; generated by 'bankpack --gen' -- do not edit"
    echo ";; SHADOW / ADDR_BANK2 are defined at link time by bankpack (-g), so the"
    echo ";; values cannot drift between this file, farrt.asm and the build."
    echo ";;"
    echo ";; Each thunk is named after the far function ITSELF (callers write"
    echo ";; play_level(3)); _far_<name> is a zero-cost alias label at the same"
    echo ";; address so pre-existing far_ callers keep linking. The natural name"
    echo ";; cannot collide with the bank's own definition because banks link"
    echo ";; SEPARATELY -- and the build reads each target's address from the"
    echo ";; owning bank's .noi only, never from a merged symbol map."
    echo "	.globl ADDR_BANK2"
    echo "	.globl SHADOW"
    echo "	.globl _fartab"
    for n in "${FAR_NAME[@]}"; do echo "	.globl _$n"; echo "	.globl _far_$n"; done
    echo "	.area _CODE"
    # self-contained JP-(IX) helper (kept in this module to avoid sdld's
    # 'definition must follow reference' cross-module ordering quirk)
    echo "_far_jp_ix:"
    echo "	jp (ix)"
    for i in "${!FAR_NAME[@]}"; do
      n="${FAR_NAME[$i]}"; seg="${FAR_SEG[$i]}"; slot=$(( i * 2 ))
      cat <<ASM
_$n:
_far_$n:
	push ix
	ld a,(SHADOW)
	push af
	ld a,#$seg
	ld (SHADOW),a
	ld (ADDR_BANK2),a
	ld ix,(_fartab + $slot)
	call _far_jp_ix
	pop af
	ld (SHADOW),a
	ld (ADDR_BANK2),a
	pop ix
	ret
ASM
    done
    echo "_fartab:"
    for n in "${FAR_NAME[@]}"; do echo "	.dw 0	; -> _$n (in its owning bank)"; done
  } > "$T"

  echo "bankpack: gen: wrote $H and $T (${#FAR_NAME[@]} far function(s))"
}

# =====================================================================
# --gen : emit far.h + _bp_far_thunks.asm (FAR lines + FAR_FN scan)
# =====================================================================
if [ "${1:-}" = "--gen" ]; then
  MANIFEST="${2:?usage: bankpack.sh --gen <manifest> [outdir] [rom_kb]}"
  OUTDIR="${3:-.}"
  GEN_ROM_KB="${4:-64}"   # only the SRAM select value depends on it (ASCII8: rom_kb/8)
  parse_manifest "$MANIFEST"
  scan_far_fn "$MANIFEST"
  sram_config "$GEN_ROM_KB"
  [ "${#FAR_NAME[@]}" -ge 1 ] || [ -n "$SRAM_KB" ] \
    || die "--gen: manifest has no FAR lines, no FAR_FN-annotated sources and no SRAM line"
  emit_far_files "$OUTDIR"
  exit 0
fi

# =====================================================================
# build / --build : link banks + assemble ROM (--build compiles first)
# =====================================================================
BUILD_ALL=0
if [ "${1:-}" = "--build" ]; then BUILD_ALL=1; shift; fi
MANIFEST="${1:?usage: bankpack.sh [--build] <manifest> <out.rom> [rom_kb]}"
OUT="${2:?usage: bankpack.sh [--build] <manifest> <out.rom> [rom_kb]}"
ROM_KB="${3:-64}"
parse_manifest "$MANIFEST"
scan_far_fn "$MANIFEST"
[ "${#SEGS[@]}" -ge 1 ] || die "manifest has no banks"

# ROM geometry checks (mapper-agnostic, need ROM_KB so they live here, not in
# parse_manifest): the image must be a whole number of segments, and every SEG
# must land INSIDE it -- place() writes with `dd seek`, which would otherwise
# silently GROW the file past ROM_KB into an image no loader accepts.
[ $(( ROM_KB * 1024 % SEGSIZE )) -eq 0 ] \
  || die "ROM size $ROM_KB KB is not a multiple of the ${MAPPER:-ASCII8} segment size ($((SEGSIZE / 1024)) KB)"
NSEG=$(( ROM_KB * 1024 / SEGSIZE ))
for s in "${SEGS[@]}"; do
  [ "$((s))" -lt "$NSEG" ] \
    || die "SEG $s is beyond ROM size ($ROM_KB KB ${MAPPER:-ASCII8} has segments 0..$((NSEG - 1)))"
done
sram_config "$ROM_KB"
# two-step flow only (--build regenerates far.h below): a far.h generated with
# a different rom_kb would carry a stale SRAM_SELECT already compiled into the
# banks -- catch the mismatch here.
if [ -n "${SRAM_SELECT:-}" ] && [ "$BUILD_ALL" -eq 0 ]; then
  GENH="$(dirname "$MANIFEST")/far.h"
  if [ -f "$GENH" ]; then
    have=$(sed -n 's/^#define SRAM_SELECT[[:space:]]*\(0x[0-9a-fA-F]*\).*/\1/p' "$GENH")
    [ -z "$have" ] || [ "$((have))" -eq "$((SRAM_SELECT))" ] \
      || die "far.h has SRAM_SELECT $have but rom_kb=$ROM_KB gives $(printf 0x%02x "$((SRAM_SELECT))") -- re-run --gen with rom_kb $ROM_KB and recompile"
  fi
fi

# ---- --build prelude: generate + compile everything the manifest names ----
# Outputs (.rel and all _bp_* intermediates) land in the CURRENT directory, like
# the plain build mode; sources are found next to the manifest.
if [ "$BUILD_ALL" -eq 1 ]; then
  MDIR=$(cd "$(dirname "$MANIFEST")" && pwd)
  # Repo root for farrt.asm and the lib/gen headers: upward search from the
  # manifest's directory (the same idiom the test runners use). Building outside
  # the repo (e.g. a /tmp scratch dir)? Point BANKPACK_FARRT at farrt.asm and
  # keep types.h next to the sources.
  BROOT="$MDIR"; while [ ! -f "$BROOT/lib/ext/farrt.asm" ] && [ "$BROOT" != / ]; do BROOT=$(dirname "$BROOT"); done
  CCINC=( -I. -I"$MDIR" )
  [ -f "$BROOT/lib/gen/types.h" ] && CCINC+=( -I"$BROOT/lib/gen" )
  # a manifest with an SRAM line means every sram.c in this project is the
  # mapper-backend one -- select it for the user (harmless for other sources).
  [ -n "$SRAM_KB" ] && CCINC+=( -DSRAM_BACKEND_MAPPER )

  if [ "${#FAR_NAME[@]}" -ge 1 ] || [ -n "$SRAM_KB" ]; then
    # far.h must sit NEXT TO the sources: a quoted #include "far.h" resolves
    # relative to the including file, wherever the build directory is.
    emit_far_files "$MDIR"
  fi
  if [ "${#FAR_NAME[@]}" -ge 1 ]; then
    sdasz80 -o _bp_far_thunks.rel "$MDIR/_bp_far_thunks.asm" || die "--build: assembling far thunks failed"
  fi

  for i in "${!SEGS[@]}"; do
    for obj in ${OBJS[$i]}; do
      case "$obj" in *.rel) b="${obj%.rel}" ;; *) continue ;; esac
      if [ "$b" = "farrt" ] && [ ! -f "$MDIR/farrt.c" ]; then
        # the resident runtime: env override > sibling copy > repo lib/ext
        src="${BANKPACK_FARRT:-}"
        if [ -z "$src" ]; then
          if [ -f "$MDIR/farrt.asm" ]; then src="$MDIR/farrt.asm"; else src="$BROOT/lib/ext/farrt.asm"; fi
        fi
        [ -f "$src" ] || die "--build: cannot locate farrt.asm (no $MDIR/farrt.asm, no repo above $MDIR; set BANKPACK_FARRT)"
        sdasz80 -o "$obj" "$src" || die "--build: assembling $src failed"
      elif [ -f "$MDIR/$b.c" ]; then
        sdcc -c -mz80 --sdcccall 1 "${CCINC[@]}" -o "$obj" "$MDIR/$b.c" || die "--build: compiling $b.c failed"
      elif [ -f "$MDIR/$b.asm" ]; then
        sdasz80 -o "$obj" "$MDIR/$b.asm" || die "--build: assembling $b.asm failed"
      elif [ -f "$obj" ]; then
        :  # prebuilt .rel with no sibling source -- use as-is
      else
        die "--build: no source for $obj (looked for $MDIR/$b.c and $MDIR/$b.asm)"
      fi
    done
  done
fi

# If far entries exist (FAR lines or FAR_FN), the generated thunks belong in the
# resident bank. The dispatch table for them is patched per entry in step 4 --
# base _fartab from the RESIDENT link, targets from each OWNING bank's link.
if [ "${#FAR_NAME[@]}" -ge 1 ]; then
  [ -f _bp_far_thunks.rel ] || die "run 'bankpack --gen' + assemble _bp_far_thunks.asm first (or use --build)"
  OBJS[0]="${OBJS[0]} _bp_far_thunks.rel"
fi

link_bank() {  # <run> <data> <base> <input-rels-and--g-opts...>
  local run="$1" data="$2" base="$3"; shift 3
  # -b _DATA only if some input DECLARES the area: sdld treats a -b for an area
  # that never appears as an error, and pure-asm residents (no C, no crt0
  # areas) have no _DATA at all.
  local dflag=() o
  for o in "$@"; do
    [ -f "$o" ] && grep -q '^A _DATA ' "$o" && { dflag=( -b "_DATA=$data" ); break; }
  done
  # NB: sdld's FIRST file arg is the OUTPUT basename (-> <base>.ihx/.map/.noi);
  # everything after it is an input. Pass an explicit output so no input .rel is
  # swallowed as the output name.
  sdldz80 -i -m -j -b _CODE="$run" "${dflag[@]}" \
          "$base" "$@" > "$base.link.log" 2>&1 \
    || { cat "$base.link.log" >&2; die "link failed (run $run)"; }
  [ -f "$base.ihx" ] || { cat "$base.link.log" >&2; die "link produced no $base.ihx (unresolved symbol? run $run)"; }
  # sdld writes CRLF .noi/.map on Windows; the arithmetic on parsed addresses
  # below would then abort. Strip CR unconditionally (no-op elsewhere).
  sed -i 's/\r$//' "$base.noi" "$base.map" 2>/dev/null || true
}

noi() { awk -v s="$1" '$1=="DEF" && $2==s {print $3}' "$2"; }

# -g SHADOW/ADDR_BANK2 assignments for a link, ONLY if some input references
# them (sdld errors on a -g assignment nobody references). farrt + the thunks
# do; plain C banks don't.
shadow_gflags() {
  local o
  for o in "$@"; do
    [ -f "$o" ] || continue
    if awk '$1=="S" && ($2=="SHADOW" || $2=="ADDR_BANK2") && $3 ~ /^Ref/ {f=1} END{exit !f}' "$o"; then
      printf -- '-g SHADOW=%s -g ADDR_BANK2=%s' "$SHADOW" "$ADDR_BANK2"
      return
    fi
  done
}

# RAM extent [start,end) of a linked image's statics: _DATA then _INITIALIZED.
# Missing/empty areas contribute nothing. Echoes "<end>" (max of both ends).
ram_end() {  # <base.noi> <fallback-start>
  local f="$1" lo="$2" sd ld si li e1 e2
  sd=$(noi s__DATA "$f");        ld=$(noi l__DATA "$f")
  si=$(noi s__INITIALIZED "$f"); li=$(noi l__INITIALIZED "$f")
  e1=$(( ${sd:-$lo} + ${ld:-0} ))
  e2=$(( ${si:-$lo} + ${li:-0} ))
  [ "$e1" -ge "$e2" ] && echo "$e1" || echo "$e2"
}

place() {  # <seg> <base>
  local seg="$1" base="$2" bin="$2.bin" sz
  sdobjcopy -I ihex -O binary "$base.ihx" "$bin" || die "objcopy failed: $base"
  sz=$(stat -c%s "$bin")
  [ "$sz" -le "$SEGSIZE" ] || die "bank (seg $seg) is $sz B > segment size $SEGSIZE"
  dd if="$bin" of="$OUT" bs=1 seek="$((seg * SEGSIZE))" conv=notrunc status=none
  printf "bankpack: seg %-3s <- %-18s (%d B, file @ 0x%04x)\n" "$seg" "$base.ihx" "$sz" "$((seg * SEGSIZE))"
}

# 0. resident helper pool: collect every SDCC runtime helper (__mulint, __divs*,
# __mullong, float, ...) referenced by ANY bank, force those into the RESIDENT
# bank (page 1, always mapped) so they exist once and are shared -- no per-bank
# duplication, no bank switch to reach them. The per-bank -g injection (step 3)
# then resolves each bank's reference to the resident copy.
RES_EXTRA=()
declare -A HELPER=()
for i in "${!SEGS[@]}"; do
  for obj in ${OBJS[$i]}; do
    [ -f "$obj" ] || continue
    while read -r name; do HELPER["$name"]=1; done \
      < <(awk '$1=="S" && $3 ~ /^Ref/ && $2 ~ /^__/ {print $2}' "$obj")
  done
done
if [ "${#HELPER[@]}" -ge 1 ]; then
  Z80LIB="$(find_z80lib)"
  [ -n "$Z80LIB" ] || die "banks reference runtime helpers (${!HELPER[*]}) but z80.lib not found; set BANKPACK_Z80LIB"
  {
    echo ";; generated by bankpack -- forces runtime helpers into the resident pool"
    for h in "${!HELPER[@]}"; do echo "	.globl $h"; done
    echo "	.area _CODE"
    echo "_bp_helper_refs:"          # a reference table forces sdld to pull each
    for h in "${!HELPER[@]}"; do echo "	.dw $h"; done
  } > _bp_helpers.asm
  sdasz80 -o _bp_helpers.rel _bp_helpers.asm || die "assembling helper refs failed"
  RES_EXTRA=( _bp_helpers.rel -k "$(dirname "$Z80LIB")" -l "$(basename "$Z80LIB")" )
  echo "bankpack: resident helper pool: ${!HELPER[*]}"
fi

# 0b. bank area-order module. A bank link has no crt0 to dictate area order, so
# sdld would follow the C module's declaration order and park _INITIALIZER after
# _INITIALIZED -- in RAM, outside the bank's ROM image. Linking this module FIRST
# in every bank pins the crt0 order: initializers in ROM right after the code.
cat > _bp_bankorder.asm <<'ASM'
;; generated by bankpack -- bank area order (initializers in ROM after the code)
	.area _HOME
	.area _CODE
	.area _INITIALIZER
	.area _GSINIT
	.area _GSFINAL
	.area _DATA
	.area _INITIALIZED
	.area _BSEG
	.area _BSS
	.area _HEAP
ASM
sdasz80 -o _bp_bankorder.rel _bp_bankorder.asm || die "assembling bank area order failed"

# 1. resident bank: link at RAMBASE, record exported symbols (full names from .noi)
link_bank "${RUNS[0]}" "$RAMBASE" "_bp_resident" ${OBJS[0]} $(shadow_gflags ${OBJS[0]}) "${RES_EXTRA[@]}"
declare -A RSYM=()
while read -r kw name val; do [ "$kw" = "DEF" ] && RSYM["$name"]="$val"; done < _bp_resident.noi
echo "bankpack: resident exports ${#RSYM[@]} symbol(s)"
NEXT=$(ram_end _bp_resident.noi "$((RAMBASE))")
printf "bankpack: statics: resident  [0x%04x,0x%04x)\n" "$((RAMBASE))" "$NEXT"

# 2. ROM image filled with 0xFF; place resident
tr '\000' '\377' < /dev/zero | head -c "$((ROM_KB * 1024))" > "$OUT"
place "${SEGS[0]}" "_bp_resident"

# 3. link + place each remaining bank, injecting only referenced resident symbols.
# Each bank's _DATA is based at the current NEXT, so every bank's statics land in
# a RAM slice DISJOINT from the resident's and from every other bank's.
DT_SEG=(); DT_SRC=(); DT_DST=(); DT_LEN=()
for i in $(seq 1 $(( ${#SEGS[@]} - 1 )) ); do
  base="_bp_bank${SEGS[$i]}"
  # Symbols DEFINED inside this bank win over the resident map: with natural-name
  # thunks the resident exports _<name> too, and injecting it into the bank that
  # DEFINES <name> (a sibling module calling its bank-mate's far function) would
  # be a duplicate definition. Skipping keeps such intra-bank calls near calls.
  declare -A BDEF=()
  for obj in ${OBJS[$i]}; do
    [ -f "$obj" ] || continue
    while read -r name; do BDEF["$name"]=1; done \
      < <(awk '$1=="S" && $3 ~ /^Def/ {print $2}' "$obj")
  done
  gopts=()
  for obj in ${OBJS[$i]}; do
    while read -r name; do
      [ -z "${BDEF[$name]:-}" ] && [ -n "${RSYM[$name]:-}" ] && gopts+=( -g "$name=${RSYM[$name]}" )
    done < <(awk '$1=="S" && $3 ~ /^Ref/ {print $2}' "$obj")
  done
  link_bank "${RUNS[$i]}" "$NEXT" "$base" _bp_bankorder.rel ${OBJS[$i]} $(shadow_gflags ${OBJS[$i]}) "${gopts[@]}"
  [ "${#gopts[@]}" -gt 0 ] && echo "bankpack: seg ${SEGS[$i]} resolved ${gopts[*]}"
  place "${SEGS[$i]}" "$base"

  # record this bank's initialized-data slice for the runtime init table
  SRC=$(noi s__INITIALIZER "$base.noi"); LEN=$(noi l__INITIALIZER "$base.noi")
  DST=$(noi s__INITIALIZED "$base.noi")
  if [ -n "$LEN" ] && [ "$((LEN))" -gt 0 ]; then
    [ "$((RUNS[$i]))" -eq "$((WINDOW))" ] \
      || die "seg ${SEGS[$i]}: initialized statics need RUN=$WINDOW (farrt copies via the bank-2 window)"
    [ "$((SRC))" -ge "$((RUNS[$i]))" ] && [ "$(( SRC + LEN ))" -le "$(( RUNS[$i] + SEGSIZE ))" ] \
      || die "seg ${SEGS[$i]}: _INITIALIZER at $SRC (+$LEN) fell outside the bank's ROM window"
    DT_SEG+=("${SEGS[$i]}"); DT_SRC+=("$((SRC))"); DT_DST+=("$((DST))"); DT_LEN+=("$((LEN))")
  fi
  BEND=$(ram_end "$base.noi" "$NEXT")
  printf "bankpack: statics: seg %-3s   [0x%04x,0x%04x)%s\n" "${SEGS[$i]}" "$NEXT" "$BEND" \
    "$( [ "$BEND" -eq "$NEXT" ] && echo '  (none)' )"
  NEXT=$BEND
done

# 3b. defend the layout: system area, SHADOW, and the program's reserved regions.
[ "$NEXT" -le "$((SYSTEM_RAM))" ] \
  || die "statics end 0x$(printf %04x "$NEXT") runs into the system area (>= $SYSTEM_RAM)"
if [ "$((SHADOW))" -ge "$((RAMBASE))" ] && [ "$((SHADOW))" -lt "$NEXT" ]; then
  die "SHADOW $SHADOW lies inside the statics region [$RAMBASE,0x$(printf %04x "$NEXT")) -- move SHADOW or RAMBASE"
fi
for r in "${RESERVES[@]:-}"; do
  [ -n "$r" ] || continue
  set -- $r; lo="$1"; hi="$2"
  if [ "$((lo))" -lt "$NEXT" ] && [ "$((hi))" -gt "$((RAMBASE))" ]; then
    die "reserved region [$lo,$hi) collides with the statics layout [$RAMBASE,0x$(printf %04x "$NEXT"))"
  fi
  if [ "$((SHADOW))" -ge "$((lo))" ] && [ "$((SHADOW))" -lt "$((hi))" ]; then
    die "SHADOW $SHADOW lies inside reserved region [$lo,$hi) -- set SHADOW in the manifest"
  fi
done

# 3c. patch the runtime init table (_bp_datatab in farrt.asm) into the ROM:
# BSS union [RAMBASE,NEXT), then one entry per bank with initialized statics.
DTAB="${RSYM[_bp_datatab]:-}"
if [ -z "$DTAB" ]; then
  if [ "${#DT_SEG[@]}" -eq 0 ] && [ "$NEXT" -eq "$((RAMBASE))" ]; then
    echo "bankpack: no statics anywhere and no _bp_datatab (old farrt.asm) -- init table skipped"
  else
    die "program has statics but the resident has no _bp_datatab -- update farrt.asm"
  fi
else
  [ "${#DT_SEG[@]}" -le "$DATATAB_MAX" ] || die "${#DT_SEG[@]} banks with init data > _bp_datatab capacity $DATATAB_MAX"
  emit_word() { printf '\\x%02x\\x%02x' $(( $1 & 0xFF )) $(( ($1 >> 8) & 0xFF )); }
  {
    printf "$(emit_word $((RAMBASE)))$(emit_word "$NEXT")"
    printf "$(printf '\\x%02x' ${#DT_SEG[@]})"
    for k in "${!DT_SEG[@]}"; do
      printf "$(printf '\\x%02x' "${DT_SEG[$k]}")$(emit_word "${DT_SRC[$k]}")$(emit_word "${DT_DST[$k]}")$(emit_word "${DT_LEN[$k]}")"
    done
  } > _bp_datatab.bin
  dtab_off=$(( SEGS[0] * SEGSIZE + (DTAB - RUNS[0]) ))
  dd if=_bp_datatab.bin of="$OUT" bs=1 seek="$dtab_off" conv=notrunc status=none
  printf "bankpack: datatab @ file 0x%04x: bss [0x%04x,0x%04x), %d init slice(s)\n" \
    "$dtab_off" "$((RAMBASE))" "$NEXT" "${#DT_SEG[@]}"
fi

# 4. patch the dispatch table (bank->bank calls) into the ROM image. Two cases:
#  - auto (FAR/FAR_FN entries, no FARTAB directive): base _fartab comes from the
#    RESIDENT link (the thunk module lives there) and each slot's TARGET address
#    is read from the OWNING bank's .noi -- the manifest says which SEG a far
#    function lives in. NEVER a merged symbol map: thunks carry the function's
#    own name, so BOTH the resident (thunk) and the owning bank (real code)
#    define _<name>, and a merged map would silently pick one of them.
#  - manual FARTAB directive: legacy merged-map lookup (hand-written tables name
#    symbols that exist exactly once across the links, no generated thunks).
if [ "${#FAR_NAME[@]}" -ge 1 ] && [ -z "$FARTAB" ]; then
  base_addr="${RSYM[_fartab]:-}"; [ -n "$base_addr" ] || die "resident link has no _fartab (far thunks missing?)"
  base_off=$(( SEGS[0] * SEGSIZE + (base_addr - RUNS[0]) ))
  for i in "${!FAR_NAME[@]}"; do
    n="${FAR_NAME[$i]}"; seg="${FAR_SEG[$i]}"; noif="_bp_bank${seg}.noi"
    [ -f "$noif" ] || die "far function '$n': seg $seg is not a linked bank (no $noif)"
    addr=$(noi "_$n" "$noif")
    [ -n "$addr" ] || die "far function '$n' is not defined in seg $seg (no _$n in $noif)"
    off=$(( base_off + i * 2 ))
    lo=$(( addr & 0xFF )); hi=$(( (addr >> 8) & 0xFF ))
    printf "$(printf '\\x%02x\\x%02x' "$lo" "$hi")" | dd of="$OUT" bs=1 seek="$off" conv=notrunc status=none
    printf "bankpack: fartab[%d] = %-14s 0x%04x seg %-3s (file @ 0x%04x)\n" "$i" "_$n" "$addr" "$seg" "$off"
  done
elif [ -n "$FARTAB" ]; then
  declare -A ALLSYM=()
  for f in _bp_*.noi; do
    while read -r kw name val; do [ "$kw" = "DEF" ] && ALLSYM["$name"]="$val"; done < "$f"
  done
  set -- $FARTAB
  base_sym="$1"; shift
  base_addr="${ALLSYM[$base_sym]:-}"; [ -n "$base_addr" ] || die "FARTAB base '$base_sym' not found"
  base_off=$(( SEGS[0] * SEGSIZE + (base_addr - RUNS[0]) ))
  slot=0
  for sym in "$@"; do
    addr="${ALLSYM[$sym]:-}"; [ -n "$addr" ] || die "FARTAB slot symbol '$sym' not found"
    off=$(( base_off + slot * 2 ))
    lo=$(( addr & 0xFF )); hi=$(( (addr >> 8) & 0xFF ))
    printf "$(printf '\\x%02x\\x%02x' "$lo" "$hi")" | dd of="$OUT" bs=1 seek="$off" conv=notrunc status=none
    printf "bankpack: fartab[%d] = %-14s 0x%04x (file @ 0x%04x)\n" "$slot" "$sym" "$addr" "$off"
    slot=$((slot + 1))
  done
fi

# 5. safety checks (link-map): enforce the invariants banking relies on. Like
# step 4: the thunk (and its far_ alias) is looked up in the RESIDENT link, the
# target in its OWNING bank's link -- per-bank .noi, never a merged map.
if [ "${#FAR_NAME[@]}" -ge 1 ]; then
  problems=0
  for i in "${!FAR_NAME[@]}"; do
    n="${FAR_NAME[$i]}"; seg="${FAR_SEG[$i]}"
    th="${RSYM[_$n]:-}"; al="${RSYM[_far_$n]:-}"
    tg=""; [ -f "_bp_bank${seg}.noi" ] && tg=$(noi "_$n" "_bp_bank${seg}.noi")
    # thunk + alias must be resident (below the window); target IN the window
    if [ -n "$th" ] && [ "$((th))" -ge "$((WINDOW))" ]; then
      echo "bankpack: CHECK FAIL: thunk _$n at $th is not resident (>= window $WINDOW)" >&2; problems=$((problems+1))
    fi
    if [ -n "$al" ] && [ "$((al))" -ge "$((WINDOW))" ]; then
      echo "bankpack: CHECK FAIL: alias _far_$n at $al is not resident (>= window $WINDOW)" >&2; problems=$((problems+1))
    fi
    if [ -n "$tg" ] && [ "$((tg))" -lt "$((WINDOW))" ]; then
      echo "bankpack: CHECK FAIL: far target _$n (seg $seg) at $tg is not in the window (< $WINDOW)" >&2; problems=$((problems+1))
    fi
  done
  # advisory: scan the resident binary for a direct call/jp into the window
  # range [0x8000,0xBFFF] -- such a branch would bypass a thunk and jump into
  # whatever segment happens to be mapped. Heuristic (may alias const data).
  hits=$(od -An -tu1 -v _bp_resident.bin 2>/dev/null | awk -v base="$((RUNS[0]))" '
    BEGIN{ split("195 205 194 202 210 218 226 234 242 250 196 204 212 220 228 236 244 252", O); for(i in O) op[O[i]]=1 }
    { for(i=1;i<=NF;i++){ b[n++]=$i } }
    END{ for(i=0;i<n-2;i++){ if(op[b[i]]){ a=b[i+1]+256*b[i+2]; if(a>=32768 && a<=49151) printf "0x%04x->0x%04x ", base+i, a } } }')
  [ -n "$hits" ] && echo "bankpack: CHECK WARN (heuristic): resident branch into window: $hits"
  [ "$problems" -eq 0 ] && echo "bankpack: safety checks OK (${#FAR_NAME[@]} thunk/target pair(s) verified)" \
                        || die "$problems safety-check failure(s)"
fi

if [ -n "${SRAM_SELECT:-}" ]; then
  printf "bankpack: SRAM %s KB on the cartridge, select 0x%02x (constants in far.h; boot with the SRAM -romtype below)\n" "$SRAM_KB" "$((SRAM_SELECT))"
fi
echo "bankpack: wrote $OUT ($((ROM_KB)) KB, $((ROM_KB * 1024 / SEGSIZE)) x $((SEGSIZE / 1024)) KB segments, ${MAPPER:-ASCII8} -- openMSX: -romtype ${SRAM_ROMTYPE:-$ROMTYPE})"
