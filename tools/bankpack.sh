#!/usr/bin/env bash
# bankpack -- turnkey ASCII-8 / ASCII-16 MegaROM builder for banked code.
#
# Pure bash + SDCC toolchain + coreutils (no Python). Two modes:
#
#   bankpack.sh --gen <manifest> [outdir]      generate far.h + far thunks
#   bankpack.sh <manifest> <out.rom> [rom_kb]  link banks + assemble the ROM
#
# The developer writes plain C and a manifest. `--gen` emits, from the manifest's
# FAR lines, a header of far-call declarations (callers just `#include "far.h"`
# and call `far_<name>(...)`) and the resident thunks + dispatch table. The build
# mode links each bank at its window address, patches the dispatch table with the
# real function addresses, and assembles the 64 KB ROM.
#
# Manifest (whitespace; '#' comments; blank lines ignored):
#   SEG   RUN      OBJ [OBJ ...]      one bank per line; first line = RESIDENT
#   FAR   SEG      <C prototype>      declare a far-callable function in SEG
#     e.g.  FAR 5 u16 farA(u16 x)
#   FARTAB base slot0 slot1 ...       (advanced/manual; auto-derived from FAR)
#   RAMBASE <addr>                    statics RAM base (default 0xC000)
#   RESERVE <lo> <hi>                 program-owned RAM [lo,hi) (e.g. __at data);
#                                     build FAILS if the layout or SHADOW touches it
#   SHADOW  <addr>                    memseg shadow byte (default 0xE020;
#                                     env BANKPACK_SHADOW overrides)
#   MAPPER  ASCII8|ASCII16            cartridge mapper (default ASCII8).
#                                     ASCII16 = 16 KB segments, so a single bank
#                                     may carry up to 16 KB of code+data. Both
#                                     mappers select the 0x8000 window through
#                                     the SAME register (0x7000), so farrt and
#                                     the generated thunks are identical.
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

ADDR_BANK2=0x7000        # page-2 window select -- same register on ASCII-8 (bank 2)
                         # and ASCII-16 (bank 2), which is what makes MAPPER a
                         # pure bankpack concern (link-time -g symbol, see below)
WINDOW=0x8000            # page-2 window base: banked code runs here, resident below
SYSTEM_RAM=0xF380        # BIOS work area: statics must stay below this
DATATAB_MAX=32           # capacity of farrt.asm's _bp_datatab (entries)
# SHADOW and ADDR_BANK2 are single-sourced HERE and injected into farrt.asm and
# the generated thunks at LINK time (-g), so the .asm files cannot drift.

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
  RAMBASE=""; RESERVES=(); MSHADOW=""; MAPPER=""
  local a b rest
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
        [ -n "$b" ] || die "MAPPER needs a type (ASCII8 or ASCII16)"
        MAPPER="$b" ;;
      *)
        [ -n "$b" ] && [ -n "$rest" ] || die "bad manifest line: $a $b $rest"
        SEGS+=("$a"); RUNS+=("$b"); OBJS+=("$rest") ;;
    esac
  done < <(sed 's/#.*//' "$1")
  RAMBASE="${RAMBASE:-0xC000}"
  SHADOW="${BANKPACK_SHADOW:-${MSHADOW:-0xE020}}"
  case "${MAPPER:-ASCII8}" in
    ASCII8)  SEGSIZE=8192 ;;
    ASCII16) SEGSIZE=16384 ;;
    *) die "unknown MAPPER '${MAPPER}' (supported: ASCII8, ASCII16)" ;;
  esac
}

# =====================================================================
# --gen : emit far.h + _bp_far_thunks.asm from the FAR lines
# =====================================================================
if [ "${1:-}" = "--gen" ]; then
  MANIFEST="${2:?usage: bankpack.sh --gen <manifest> [outdir]}"
  OUTDIR="${3:-.}"
  parse_manifest "$MANIFEST"
  [ "${#FAR_NAME[@]}" -ge 1 ] || die "--gen: manifest has no FAR lines"

  H="$OUTDIR/far.h"; T="$OUTDIR/_bp_far_thunks.asm"
  {
    echo "// generated by 'bankpack --gen' -- do not edit"
    echo "#ifndef BANKPACK_FAR_H"
    echo "#define BANKPACK_FAR_H"
    echo '#include "types.h"'
    for i in "${!FAR_NAME[@]}"; do
      # prefix the function name with far_ in its prototype
      printf 'extern %s;\n' \
        "$(printf '%s' "${FAR_PROTO[$i]}" | sed 's/\([A-Za-z_][A-Za-z0-9_]*\)[[:space:]]*(/far_\1(/')"
    done
    echo "#endif"
  } > "$H"

  {
    echo ";; generated by 'bankpack --gen' -- do not edit"
    echo ";; SHADOW / ADDR_BANK2 are defined at link time by bankpack (-g), so the"
    echo ";; values cannot drift between this file, farrt.asm and the build."
    echo "	.globl ADDR_BANK2"
    echo "	.globl SHADOW"
    echo "	.globl _fartab"
    for n in "${FAR_NAME[@]}"; do echo "	.globl _far_$n"; done
    echo "	.area _CODE"
    # self-contained JP-(IX) helper (kept in this module to avoid sdld's
    # 'definition must follow reference' cross-module ordering quirk)
    echo "_far_jp_ix:"
    echo "	jp (ix)"
    for i in "${!FAR_NAME[@]}"; do
      n="${FAR_NAME[$i]}"; seg="${FAR_SEG[$i]}"; slot=$(( i * 2 ))
      cat <<ASM
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
    for n in "${FAR_NAME[@]}"; do echo "	.dw 0	; -> _$n"; done
  } > "$T"

  echo "bankpack --gen: wrote $H and $T (${#FAR_NAME[@]} far function(s))"
  exit 0
fi

# =====================================================================
# build : link banks + assemble ROM
# =====================================================================
MANIFEST="${1:?usage: bankpack.sh <manifest> <out.rom> [rom_kb]}"
OUT="${2:?usage: bankpack.sh <manifest> <out.rom> [rom_kb]}"
ROM_KB="${3:-64}"
parse_manifest "$MANIFEST"
[ "${#SEGS[@]}" -ge 1 ] || die "manifest has no banks"

# If FAR lines exist, the generated thunks belong in the resident bank, and the
# dispatch table is auto-derived (base _fartab, slots _<name> in FAR order).
if [ "${#FAR_NAME[@]}" -ge 1 ]; then
  [ -f _bp_far_thunks.rel ] || die "run 'bankpack --gen' + assemble _bp_far_thunks.asm first"
  OBJS[0]="${OBJS[0]} _bp_far_thunks.rel"
  if [ -z "$FARTAB" ]; then
    FARTAB="_fartab"
    for n in "${FAR_NAME[@]}"; do FARTAB="$FARTAB _$n"; done
  fi
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
  gopts=()
  for obj in ${OBJS[$i]}; do
    while read -r name; do
      [ -n "${RSYM[$name]:-}" ] && gopts+=( -g "$name=${RSYM[$name]}" )
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

# 4. patch the dispatch table (bank->bank calls) into the ROM image
if [ -n "$FARTAB" ]; then
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

# 5. safety checks (link-map): enforce the invariants banking relies on.
if [ "${#FAR_NAME[@]}" -ge 1 ]; then
  declare -A CHKSYM=()
  for f in _bp_*.noi; do
    while read -r kw name val; do [ "$kw" = "DEF" ] && CHKSYM["$name"]="$val"; done < "$f"
  done
  problems=0
  for n in "${FAR_NAME[@]}"; do
    th="${CHKSYM[_far_$n]:-}"; tg="${CHKSYM[_$n]:-}"
    # thunk must be resident (below the window); target must be IN the window
    if [ -n "$th" ] && [ "$((th))" -ge "$((WINDOW))" ]; then
      echo "bankpack: CHECK FAIL: thunk _far_$n at $th is not resident (>= window $WINDOW)" >&2; problems=$((problems+1))
    fi
    if [ -n "$tg" ] && [ "$((tg))" -lt "$((WINDOW))" ]; then
      echo "bankpack: CHECK FAIL: far target _$n at $tg is not in the window (< $WINDOW)" >&2; problems=$((problems+1))
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

echo "bankpack: wrote $OUT ($((ROM_KB)) KB, $((ROM_KB * 1024 / SEGSIZE)) x $((SEGSIZE / 1024)) KB segments, ${MAPPER:-ASCII8})"
