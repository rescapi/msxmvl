#!/usr/bin/env bash
# mkbootdsk -- turn a manifest of payloads + data blobs into a self-booting FAT12
# disk image. Pure bash + coreutils (the bankpack.sh ethos; MSXgl needs a compiled
# C++ tool for the same job).
#
#   mkbootdsk.sh <manifest> <out.dsk> [720|360]
#
# The image boots with NO MSX-DOS on it: sector 0 is our boot sector
# (lib/ext/bootsector.asm), which the disk ROM kernel runs and which FCB-loads the
# one `load` payload to 0x0100 and jumps to it. Every entry is also written as a
# real, contiguous FAT12 file, so the image mounts host-side and is listable.
#
# On top of the plain filesystem we lay a mini-diskOS SECTOR INDEX in the unused
# second half of sector 0 (byte 0x100+, which the boot ROM does not copy to RAM):
# id -> {start sector, sector count, byte length}, computed HERE at build time.
# lib/ext/diskos.c reads a blob straight off its sectors with no FAT walk -- that
# precomputation is the "faster + smaller than DOS" win. See docs/Boot-Disk.md.
#
# MANIFEST (whitespace-separated; '#' comments and blank lines ignored):
#   <file>  load      <addr>    the boot payload (exactly one). Its 8.3 name is
#                               patched into the boot FCB; <addr> must be 0x0100
#                               (the crt0_bootdisk TPA model).
#   <file>  resident  [-]       a data blob: written as a file AND indexed, fetched
#                               at runtime by DiskOS_Load(id, dst). Stays on disk.
#
# Each entry's runtime id is its 0-based position in the manifest (printed in the
# id map at the end). <file> is a path to the already-built binary; its basename
# becomes the 8.3 on-disk name (must fit 8.3, letters/digits only).
set -uo pipefail

die() { echo "mkbootdsk: $*" >&2; exit 1; }

MANIFEST="${1:?usage: mkbootdsk.sh <manifest> <out.dsk> [720|360]}"
OUT="${2:?usage: mkbootdsk.sh <manifest> <out.dsk> [720|360]}"
SIZE="${3:-720}"

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BOOTSRC="$ROOT/lib/ext/bootsector.asm"
PASMO="${PASMO:-$(command -v pasmo || echo /home/remco/tools/pasmo)}"
[ -f "$BOOTSRC" ] || die "cannot find $BOOTSRC"
[ -x "$PASMO" ] || die "pasmo not found (set \$PASMO)"

#=============================================================================
# GEOMETRY (standard MSX FAT12)
#=============================================================================
BPS=512; SPC=2; RESERVED=1; NFATS=2; ROOTENTS=112; SPT=9
case "$SIZE" in
  720) TOTAL=1440; SPF=3; MEDIA=0xF9; HEADS=2 ;;
  360) TOTAL=720;  SPF=2; MEDIA=0xF8; HEADS=1 ;;
  *) die "size must be 720 or 360 (got '$SIZE')" ;;
esac
ROOTSECS=$(( ROOTENTS * 32 / BPS ))          # 112*32/512 = 7
FAT1_SEC=$RESERVED                            # sector 1
FAT2_SEC=$(( RESERVED + SPF ))
ROOT_SEC=$(( RESERVED + NFATS * SPF ))        # 720K: 1+2*3 = 7
DATA_SEC=$(( ROOT_SEC + ROOTSECS ))           # 720K: 7+7  = 14
CLU_BYTES=$(( SPC * BPS ))                     # 1024
NENT=$(( SPF * BPS * 2 / 3 ))                  # FAT capacity in 12-bit entries

#=============================================================================
# byte-string helpers -- emit printf-escaped \xNN sequences (interpret with %b)
#=============================================================================
b8()   { printf '\\x%02x' "$(( $1 & 0xFF ))"; }
le16() { printf '\\x%02x\\x%02x' "$(( $1 & 0xFF ))" "$(( ($1 >> 8) & 0xFF ))"; }
le32() { printf '\\x%02x\\x%02x\\x%02x\\x%02x' \
           "$(( $1 & 0xFF ))" "$(( ($1>>8) & 0xFF ))" \
           "$(( ($1>>16) & 0xFF ))" "$(( ($1>>24) & 0xFF ))"; }
# write the raw bytes of a printf-escaped string $2 at byte offset $1 of $OUT
poke() { printf "%b" "$2" | dd of="$OUT" bs=1 seek="$(( $1 ))" conv=notrunc status=none; }

# derive an 11-byte 8.3 FAT name (space padded) from a path's basename
fatname() {  # <path> -> 11-char string
  local b n e
  b=$(basename "$1"); b=${b^^}
  n=${b%.*}; e=""
  case "$b" in *.*) e=${b##*.} ;; esac
  [ "${#n}" -ge 1 ] && [ "${#n}" -le 8 ] || die "'$1': name part must be 1..8 chars (got '$n')"
  [ "${#e}" -le 3 ] || die "'$1': extension must be 0..3 chars (got '$e')"
  case "$n$e" in *[!A-Z0-9_]*) die "'$1': 8.3 name may use only A-Z 0-9 _ (got '$b')" ;; esac
  printf '%-8s%-3s' "$n" "$e"
}

#=============================================================================
# 1. parse the manifest
#=============================================================================
PATHS=(); KINDS=(); ARGS=(); FNAMES=()
LOAD_IDX=-1
while read -r f kind arg _; do
  [ -z "${f:-}" ] && continue
  case "$f" in \#*) continue ;; esac
  [ -n "${kind:-}" ] || die "manifest line '$f' has no kind (load|resident)"
  # <file> is relative to the manifest's directory unless absolute
  case "$f" in /*) p="$f" ;; *) p="$(cd "$(dirname "$MANIFEST")" && pwd)/$f" ;; esac
  [ -f "$p" ] || die "no such file: $p (from manifest line '$f')"
  case "$kind" in
    load)
      [ "$LOAD_IDX" -lt 0 ] || die "more than one 'load' entry (only one boot payload allowed)"
      [ "${arg:-}" = "0x0100" ] || [ "${arg:-}" = "0x100" ] \
        || die "'load' addr must be 0x0100 (the crt0_bootdisk TPA model), got '${arg:-}'"
      LOAD_IDX=${#PATHS[@]} ;;
    resident) : ;;
    *) die "unknown kind '$kind' (use load|resident)" ;;
  esac
  PATHS+=("$p"); KINDS+=("$kind"); ARGS+=("${arg:-}"); FNAMES+=("$(fatname "$p")")
done < "$MANIFEST"

[ "${#PATHS[@]}" -ge 1 ] || die "manifest has no entries"
[ "$LOAD_IDX" -ge 0 ] || die "manifest has no 'load' entry (need exactly one boot payload)"

#=============================================================================
# 2. lay every entry out contiguously from cluster 2; record sector index
#=============================================================================
START_SEC=(); SEC_CNT=(); LENGTHS=(); FILE_CLU=()
clu=2
for i in "${!PATHS[@]}"; do
  sz=$(stat -c%s "${PATHS[$i]}")
  [ "$sz" -ge 1 ] || die "'${PATHS[$i]}' is empty"
  nclu=$(( (sz + CLU_BYTES - 1) / CLU_BYTES ))          # whole clusters
  nsec=$(( (sz + BPS - 1) / BPS ))                        # used sectors (index)
  FILE_CLU+=("$clu")
  START_SEC+=("$(( DATA_SEC + (clu - 2) * SPC ))")
  SEC_CNT+=("$nsec")
  LENGTHS+=("$sz")
  clu=$(( clu + nclu ))
done
LAST_CLU=$(( clu - 1 ))
MAXCLU=$(( 2 + (TOTAL - DATA_SEC) / SPC - 1 ))
[ "$LAST_CLU" -le "$MAXCLU" ] \
  || die "payloads need $((LAST_CLU-1)) clusters but the ${SIZE}K image only has $((MAXCLU-1))"

NIDX=${#PATHS[@]}
[ "$(( 3 + NIDX * 7 ))" -le 256 ] || die "$NIDX entries overflow the 256-byte sector index (max 36)"

# The boot sector's phase-2 (>16 KB) loader stages through 0x8000, so the payload's
# page-1 tail must land below 0x8000: dest_end = 0x3F00 + (sectors-31)*512 <= 0x8000
# => <= 63 sectors (~31.5 KB). Larger flat payloads are not supported (stream the
# rest as `resident` blobs instead -- see docs/Boot-Disk.md).
[ "${SEC_CNT[$LOAD_IDX]}" -le 63 ] \
  || die "load payload is ${SEC_CNT[$LOAD_IDX]} sectors; the phase-2 loader supports <=63 (~31.5 KB). Split data into 'resident' blobs."

#=============================================================================
# 3. create the zero-filled image
#=============================================================================
head -c "$(( TOTAL * BPS ))" /dev/zero > "$OUT" || die "cannot create $OUT"

#=============================================================================
# 4. sector 0 = boot sector + patched geometry + patched payload name + index
#=============================================================================
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
"$PASMO" --bin "$BOOTSRC" "$TMP/boot.bin" "$TMP/boot.sym" 2>"$TMP/asm.log" \
  || { cat "$TMP/asm.log" >&2; die "assembling boot sector failed"; }
bsz=$(stat -c%s "$TMP/boot.bin")
[ "$bsz" -le 256 ] || die "boot code is $bsz B (> 256; would collide with the index at 0x100)"
dd if="$TMP/boot.bin" of="$OUT" bs=1 conv=notrunc status=none      # boot code at 0

# patch the BPB geometry (no-op for 720K, real for 360K; single-sourced here)
poke 0x15 "$(b8 $MEDIA)"           # media descriptor
poke 0x13 "$(le16 $TOTAL)"         # total sectors
poke 0x16 "$(le16 $SPF)"           # sectors per FAT
poke 0x1A "$(b8 $HEADS)"           # heads

# resolve a boot.sym label to a byte offset within the sector (org 0xC000)
symoff() { local s; s=$(awk -v n="$1" '$1==n{print $3}' "$TMP/boot.sym"); [ -n "$s" ] || die "boot.sym has no $1"; echo $(( 0x${s%H} - 0xC000 )); }

# patch the boot FCB name (fcb_name symbol) with the load payload's 8.3 name
name="${FNAMES[$LOAD_IDX]}"
nesc=""; for ((c=0; c<11; c++)); do nesc+=$(printf '\\x%02x' "'${name:$c:1}"); done
poke "$(symoff fcb_name)" "$nesc"

# patch the payload's start sector + sector count (phase-2 >16 KB loader uses them)
poke "$(symoff bs_start_sec)" "$(le16 ${START_SEC[$LOAD_IDX]})"
poke "$(symoff bs_total_sec)" "$(le16 ${SEC_CNT[$LOAD_IDX]})"

# mini-diskOS sector index at byte 0x100 (unused half of the boot sector)
idx="$(b8 0x44)$(b8 0x4B)$(b8 $NIDX)"          # magic 'D','K' + entry count
for i in "${!PATHS[@]}"; do
  idx+="$(b8 $i)$(le16 ${START_SEC[$i]})$(le16 ${SEC_CNT[$i]})$(le16 ${LENGTHS[$i]})"
done
poke 0x100 "$idx"

#=============================================================================
# 5. build the FAT (both copies). Contiguous files => trivial ascending chains.
#=============================================================================
declare -a FATB
for ((k=0; k<SPF*BPS; k++)); do FATB[$k]=0; done
set_fat() {  # <entry-index> <12-bit value>
  local idx=$1 val=$2 k base
  k=$(( idx / 2 )); base=$(( 3 * k ))
  if [ $(( idx % 2 )) -eq 0 ]; then
    FATB[$base]=$(( val & 0xFF ))
    FATB[$((base+1))]=$(( (FATB[$((base+1))] & 0xF0) | ((val >> 8) & 0x0F) ))
  else
    FATB[$((base+1))]=$(( (FATB[$((base+1))] & 0x0F) | ((val & 0x0F) << 4) ))
    FATB[$((base+2))]=$(( (val >> 4) & 0xFF ))
  fi
}
set_fat 0 $(( 0xF00 | (MEDIA & 0xFF) ))        # FAT[0] = media in the low byte
set_fat 1 0xFFF                                 # FAT[1] = reserved / EOF
for i in "${!PATHS[@]}"; do                     # one ascending chain per file
  c=${FILE_CLU[$i]}
  nclu=$(( (LENGTHS[$i] + CLU_BYTES - 1) / CLU_BYTES ))
  for ((j=0; j<nclu; j++)); do
    if [ "$j" -eq $(( nclu - 1 )) ]; then set_fat $(( c + j )) 0xFFF   # last -> EOF
    else set_fat $(( c + j )) $(( c + j + 1 )); fi
  done
done
fatesc=$(printf '\\x%02x' "${FATB[@]}")         # one printf call, all bytes
poke "$(( FAT1_SEC * BPS ))" "$fatesc"
poke "$(( FAT2_SEC * BPS ))" "$fatesc"

#=============================================================================
# 6. root directory -- one 32-byte entry per file
#=============================================================================
for i in "${!PATHS[@]}"; do
  name="${FNAMES[$i]}"
  ent=""; for ((c=0; c<11; c++)); do ent+=$(printf '\\x%02x' "'${name:$c:1}"); done
  ent+="$(b8 0x20)"                    # attr = archive (offset 11)
  ent+="$(printf '\\x00%.0s' {1..12})" # offsets 12..23: create/access stamps, cluster-high
  ent+="$(le16 0x0021)"                # write date = 1980-01-01 (offset 24)
  ent+="$(le16 ${FILE_CLU[$i]})"       # first cluster (offset 26)
  ent+="$(le32 ${LENGTHS[$i]})"        # file size (offset 28)
  poke "$(( ROOT_SEC * BPS + i * 32 ))" "$ent"
done

#=============================================================================
# 7. write file data at each file's start sector
#=============================================================================
for i in "${!PATHS[@]}"; do
  dd if="${PATHS[$i]}" of="$OUT" bs=$BPS seek="${START_SEC[$i]}" conv=notrunc status=none
done

#=============================================================================
# report + id map
#=============================================================================
echo "mkbootdsk: wrote $OUT (${SIZE} KB FAT12, $TOTAL sectors; data from sector $DATA_SEC)"
printf 'mkbootdsk: %-3s %-11s %-9s %-8s %s\n' id name kind sector "len(bytes)"
for i in "${!PATHS[@]}"; do
  extra=""; [ "$i" -eq "$LOAD_IDX" ] && extra="@${ARGS[$i]}"
  printf 'mkbootdsk: %-3s %-11s %-9s %-8s %s %s\n' \
    "$i" "${FNAMES[$i]}" "${KINDS[$i]}" "${START_SEC[$i]}" "${LENGTHS[$i]}" "$extra"
done
