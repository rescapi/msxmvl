#!/usr/bin/env bash
# Acceptance test for BIG-ROM builds (docs/Code-Banking-Mappers.md): a 512 KB
# ASCII-8 image (64 segments) with far code + writable statics in SEG 63 — the
# LAST segment — asserted on C-BIOS_MSX1 AND C-BIOS_MSX2, the ROM file size
# asserted exact, and the out-of-range case (a bank on SEG 64) asserted to
# FAIL the build with the "beyond ROM size" check.
set -uo pipefail
export SDL_VIDEODRIVER=dummy
DIR=$(cd "$(dirname "$0")" && pwd)
ROOT="$DIR"; while [ ! -f "$ROOT/lib/ext/farrt.asm" ] && [ "$ROOT" != / ]; do ROOT=$(dirname "$ROOT"); done
OMX="${OPENMSX:-$(command -v openmsx || echo /home/remco/tools/openmsx/bin/openmsx)}"
BP="$ROOT/tools/bankpack.sh"
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT

cp "$ROOT/lib/gen/types.h" "$DIR/main.c" "$DIR/bankmid.c" "$DIR/bankz.c" "$DIR/bank.manifest" "$W/"
cd "$W"
export BANKPACK_FARRT="$ROOT/lib/ext/farrt.asm"

# 1. one --build command, 512 KB image
bash "$BP" --build bank.manifest game.rom 512 || { echo "BUILD FAIL"; exit 2; }

# 2. the image must be EXACTLY 512 KB — a `dd seek` past the end would have
#    silently grown it (precisely what the "beyond ROM size" check refuses)
fail=0
SZ=$(stat -c%s game.rom)
if [ "$SZ" -eq $((512 * 1024)) ]; then
  echo "  ROM size: PASS (524288 B)"
else
  echo "  ROM size: FAIL ($SZ B != 524288)"; fail=1
fi

# 3. run on both machine generations
MEND=$(awk '$1=="DEF" && $2=="_mark_end"{print $3}' _bp_resident.noi)
[ -n "$MEND" ] || { echo "no _mark_end"; exit 2; }
for MACHINE in C-BIOS_MSX1 C-BIOS_MSX2; do
  TCL=$(mktemp)
  cat > "$TCL" <<TCL
set renderer none
set scheduler_mode fast
set throttle off
after realtime 25 { puts stderr "OUT=TIMEOUT"; exit 1 }
carta $W/game.rom -romtype ASCII8
debug set_bp $MEND {} {
  set b {}
  for {set i 0} {\$i < 8} {incr i} { lappend b [format %02x [debug read memory [expr {0xE000+\$i}]]] }
  puts stderr "OUT=RES=[join \$b {}]"
  exit 0
}
TCL
  RAW=$(timeout 30 "$OMX" -machine "$MACHINE" -script "$TCL" 2>&1 | sed -n 's/.*OUT=//p')
  rm -f "$TCL"
  RES=$(echo "$RAW" | sed -n 's/^RES=\([0-9a-f]*\).*/\1/p' | head -1)
  if [ "${RES:0:2}" = "a5" ]; then
    echo "  $MACHINE: PASS  R[]=$RES"
  else
    echo "  $MACHINE: FAIL  R[]=${RES:-<none>} ($RAW)"; fail=1
  fi
done

# 4. negative: a bank on SEG 64 (one past a 512 KB ASCII-8 image) must be a
#    BUILD ERROR naming the geometry, not a silently grown file.
sed 's/^  63 /  64 /' bank.manifest > bank.bad.manifest
if ERR=$(bash "$BP" --build bank.bad.manifest bad.rom 512 2>&1); then
  echo "  SEG-beyond-ROM: FAIL (build was accepted)"; fail=1
elif echo "$ERR" | grep -q "beyond ROM size"; then
  echo "  SEG-beyond-ROM: PASS (build rejected: beyond ROM size)"
else
  echo "  SEG-beyond-ROM: FAIL (wrong message: $ERR)"; fail=1
fi

[ "$fail" -eq 0 ] && { echo "  RESULT: PASS"; exit 0; } || { echo "  RESULT: FAIL"; exit 1; }
