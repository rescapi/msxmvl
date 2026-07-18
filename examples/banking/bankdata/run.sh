#!/usr/bin/env bash
# Acceptance test for the bankpack C-RUNTIME DATA MODEL (docs/Code-Banking-Data-Model.md):
# resident + two banks, EACH with initialized statics and BSS, plus an __at reserved
# region. Asserts on C-BIOS_MSX1 AND C-BIOS_MSX2 that every static holds its initial
# value, BSS is zeroed, and bank statics don't alias — then asserts the build FAILS
# on a deliberate reserved-region collision.
set -uo pipefail
export SDL_VIDEODRIVER=dummy
DIR=$(cd "$(dirname "$0")" && pwd)
ROOT="$DIR"; while [ ! -f "$ROOT/lib/ext/farrt.asm" ] && [ "$ROOT" != / ]; do ROOT=$(dirname "$ROOT"); done
OMX="${OPENMSX:-$(command -v openmsx || echo /home/remco/tools/openmsx/bin/openmsx)}"
BP="$ROOT/tools/bankpack.sh"
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT

cp "$ROOT/lib/gen/types.h" "$W/"
cp "$ROOT/lib/ext/farrt.asm" "$W/"
cp "$DIR/main.c" "$DIR/bankA.c" "$DIR/bankA2.c" "$DIR/bankB.c" "$DIR/bank.manifest" "$W/"
cd "$W"

# 1. generate far.h + thunks; compile / assemble everything
bash "$BP" --gen bank.manifest . || { echo "GEN FAIL"; exit 1; }
sdasz80 -o farrt.rel farrt.asm                       || { echo "ASM farrt FAIL"; exit 1; }
sdasz80 -o _bp_far_thunks.rel _bp_far_thunks.asm     || { echo "ASM thunks FAIL"; exit 1; }
for c in main bankA bankA2 bankB; do
  sdcc -c -mz80 --sdcccall 1 -I. -o "$c.rel" "$c.c"  || { echo "CC $c FAIL"; exit 1; }
done

# 2. build the ROM (this is where the coordinated DATA/BSS layout happens)
bash "$BP" bank.manifest game.rom 64 || { echo "BANKPACK FAIL"; exit 2; }

# 3. run on both machine generations
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

# 4. negative: a RESERVE that collides with the coordinated layout must be a
#    BUILD ERROR, not silent corruption.
sed 's/^RESERVE .*/RESERVE 0xC000 0xC100/' bank.manifest > bank.collide.manifest
if bash "$BP" bank.collide.manifest collide.rom 64 >/dev/null 2>&1; then
  echo "  reserved-region collision: FAIL (build was accepted)"; fail=1
else
  echo "  reserved-region collision: PASS (build rejected)"
fi

[ "$fail" -eq 0 ] && { echo "  RESULT: PASS"; exit 0; } || { echo "  RESULT: FAIL"; exit 1; }
