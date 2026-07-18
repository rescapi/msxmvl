#!/usr/bin/env bash
# Acceptance test for MAPPER KONAMI (docs/Code-Banking-Mappers.md): the turnkey
# far-call program built as a plain Konami (Konami4) image (window select
# 0x8000; 0x4000-0x5FFF hardware-fixed to segment 0), booted with
# -romtype Konami on C-BIOS_MSX1 AND C-BIOS_MSX2 — plus the mapper-wide
# resident check asserted: a resident on SEG 1 must FAIL the build.
set -uo pipefail
export SDL_VIDEODRIVER=dummy
DIR=$(cd "$(dirname "$0")" && pwd)
ROOT="$DIR"; while [ ! -f "$ROOT/lib/ext/farrt.asm" ] && [ "$ROOT" != / ]; do ROOT=$(dirname "$ROOT"); done
OMX="${OPENMSX:-$(command -v openmsx || echo /home/remco/tools/openmsx/bin/openmsx)}"
BP="$ROOT/tools/bankpack.sh"
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT

cp "$ROOT/lib/gen/types.h" "$DIR/main.c" "$DIR/bankA.c" "$DIR/bankB.c" "$DIR/bank.manifest" "$W/"
cd "$W"
export BANKPACK_FARRT="$ROOT/lib/ext/farrt.asm"

# 1. one --build command
bash "$BP" --build bank.manifest game.rom 64 || { echo "BUILD FAIL"; exit 2; }

# 2. run on both machine generations
MEND=$(awk '$1=="DEF" && $2=="_mark_end"{print $3}' _bp_resident.noi)
[ -n "$MEND" ] || { echo "no _mark_end"; exit 2; }
fail=0
for MACHINE in C-BIOS_MSX1 C-BIOS_MSX2; do
  TCL=$(mktemp)
  cat > "$TCL" <<TCL
set renderer none
set scheduler_mode fast
set throttle off
after realtime 25 { puts stderr "OUT=TIMEOUT"; exit 1 }
carta $W/game.rom -romtype Konami
debug set_bp $MEND {} {
  set b {}
  for {set i 0} {\$i < 3} {incr i} { lappend b [format %02x [debug read memory [expr {0xE000+\$i}]]] }
  puts stderr "OUT=RES=[join \$b {}]"
  exit 0
}
TCL
  RAW=$(timeout 30 "$OMX" -machine "$MACHINE" -script "$TCL" 2>&1 | sed -n 's/.*OUT=//p')
  rm -f "$TCL"
  RES=$(echo "$RAW" | sed -n 's/^RES=\([0-9a-f]*\).*/\1/p' | head -1)
  if [ "${RES:0:2}" = "a5" ]; then
    echo "  $MACHINE: PASS  R[]=$RES (play_level(3)=$((0x${RES:4:2}${RES:2:2})))"
  else
    echo "  $MACHINE: FAIL  R[]=${RES:-<none>} ($RAW)"; fail=1
  fi
done

# 3. negative: the resident on any SEG but 0 must be a BUILD ERROR — on this
#    mapper 0x4000-0x5FFF is hardware-fixed to segment 0, a resident placed on
#    SEG 1 would simply never be mapped (the check is mapper-wide; KONAMI is
#    where it stops being a convention and becomes physics).
sed 's/^  0 /  1 /' bank.manifest > bank.bad.manifest
if ERR=$(bash "$BP" --build bank.bad.manifest bad.rom 64 2>&1); then
  echo "  resident-not-SEG-0: FAIL (build was accepted)"; fail=1
elif echo "$ERR" | grep -q "resident bank must be SEG 0"; then
  echo "  resident-not-SEG-0: PASS (build rejected with a clear message)"
else
  echo "  resident-not-SEG-0: FAIL (wrong message: $ERR)"; fail=1
fi

[ "$fail" -eq 0 ] && { echo "  RESULT: PASS"; exit 0; } || { echo "  RESULT: FAIL"; exit 1; }
