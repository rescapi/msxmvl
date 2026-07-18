#!/usr/bin/env bash
# Build the bank->bank-call MegaROM with bankpack, boot it on C-BIOS_MSX2, and
# assert that farA(5) [segment 5] correctly called farB [segment 6] through the
# resident dispatch table, with the intermediate segment restored on the way back.
#
#   farA(5) = (5 + 1 + 100) + 10 = 116 (0x74)
#   RES[0]=0xA5 pass, RES[1]=lo(0x74), RES[2]=hi(0x00)
set -uo pipefail
export SDL_VIDEODRIVER=dummy
DIR=$(cd "$(dirname "$0")" && pwd)
ROOT="$DIR"; while [ ! -f "$ROOT/lib/ext/farrt.asm" ] && [ "$ROOT" != / ]; do ROOT=$(dirname "$ROOT"); done
OMX="${OPENMSX:-$(command -v openmsx || echo /home/remco/tools/openmsx/bin/openmsx)}"
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT

# --- stage objects: assemble resident, compile the two C banks ---
sdasz80 -o "$W/residentrt.rel" "$DIR/residentrt.asm" || { echo "ASM resident FAIL"; exit 1; }
sdcc -c -mz80 --sdcccall 1 -o "$W/bankA.rel" "$DIR/bankA.c" || { echo "CC bankA FAIL"; exit 1; }
sdcc -c -mz80 --sdcccall 1 -o "$W/bankB.rel" "$DIR/bankB.c" || { echo "CC bankB FAIL"; exit 1; }
cp "$DIR/bank.manifest" "$W/"

( cd "$W" && bash "$ROOT/tools/bankpack.sh" bank.manifest game.rom 64 ) || { echo "BANKPACK FAIL"; exit 2; }

ROM="$W/game.rom"
MEND=$(awk '$1=="DEF" && $2=="_mark_end"{print $3}' "$W/_bp_resident.noi")
[ -n "$MEND" ] || { echo "no _mark_end in resident .noi"; exit 2; }
echo "  _mark_end @ $MEND"

TCL=$(mktemp)
cat > "$TCL" <<TCL
set renderer none
set scheduler_mode fast
set throttle off
after realtime 25 { puts stderr "OUT=TIMEOUT"; exit 1 }
carta $ROM -romtype ASCII8
debug set_bp $MEND {} {
  set b {}
  for {set i 0} {\$i < 3} {incr i} { lappend b [format %02x [debug read memory [expr {0xE000+\$i}]]] }
  puts stderr "OUT=RES=[join \$b {}]"
  exit 0
}
TCL
RAW=$(timeout 30 "$OMX" -machine C-BIOS_MSX2 -script "$TCL" 2>&1 | sed -n 's/.*OUT=//p')
rm -f "$TCL"

RES=$(echo "$RAW" | sed -n 's/^RES=\([0-9a-f]*\).*/\1/p' | head -1)
echo "  RES[] = ${RES:-<none>}"
[ -n "$RES" ] || { echo "  RESULT: FAIL (no output)"; exit 1; }
r0=${RES:0:2}; r1=${RES:2:2}; r2=${RES:4:2}
echo "    RES[0] verdict = $r0 (want a5)"
echo "    RES[1] lo      = $r1 (want 74)"
echo "    RES[2] hi      = $r2 (want 00)"
if [ "$r0" = "a5" ]; then echo "  RESULT: PASS  (farA->farB bank->bank call verified)"; exit 0
else echo "  RESULT: FAIL"; exit 1; fi
