#!/usr/bin/env bash
# Turnkey banked-ROM build: the developer wrote only plain C + bank.manifest.
# This runner shows the full turnkey flow and asserts the result on hardware.
#
#   1. bankpack --gen  -> far.h (call decls) + _bp_far_thunks.asm (thunks + table)
#   2. compile C (banks + main) and assemble the runtime + generated thunks
#   3. bankpack build  -> link each bank, patch the dispatch table, assemble ROM
#   4. boot on C-BIOS_MSX2, assert play_level(3) == 350
set -uo pipefail
export SDL_VIDEODRIVER=dummy
DIR=$(cd "$(dirname "$0")" && pwd)
ROOT="$DIR"; while [ ! -f "$ROOT/lib/ext/farrt.asm" ] && [ "$ROOT" != / ]; do ROOT=$(dirname "$ROOT"); done
OMX="${OPENMSX:-$(command -v openmsx || echo /home/remco/tools/openmsx/bin/openmsx)}"
BP="$ROOT/tools/bankpack.sh"
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT

cp "$ROOT/lib/gen/types.h" "$W/"
cp "$ROOT/lib/ext/farrt.asm" "$W/"
cp "$DIR/main.c" "$DIR/bankA.c" "$DIR/bankB.c" "$DIR/bank.manifest" "$W/"
cd "$W"

# 1. generate far.h + far thunks from the manifest's FAR lines
bash "$BP" --gen bank.manifest . || { echo "GEN FAIL"; exit 1; }

# 2. compile / assemble everything (far.h now available)
sdasz80 -o farrt.rel farrt.asm                       || { echo "ASM farrt FAIL"; exit 1; }
sdasz80 -o _bp_far_thunks.rel _bp_far_thunks.asm     || { echo "ASM thunks FAIL"; exit 1; }
for c in main bankA bankB; do
  sdcc -c -mz80 --sdcccall 1 -I. -o "$c.rel" "$c.c"  || { echo "CC $c FAIL"; exit 1; }
done

# 3. build + patch the ROM
bash "$BP" bank.manifest game.rom 64 || { echo "BANKPACK FAIL"; exit 2; }

# 4. run
MEND=$(awk '$1=="DEF" && $2=="_mark_end"{print $3}' _bp_resident.noi)
[ -n "$MEND" ] || { echo "no _mark_end"; exit 2; }
echo "  _mark_end @ $MEND"
TCL=$(mktemp)
cat > "$TCL" <<TCL
set renderer none
set scheduler_mode fast
set throttle off
after realtime 25 { puts stderr "OUT=TIMEOUT"; exit 1 }
carta $W/game.rom -romtype ASCII8
debug set_bp $MEND {} {
  set b {}
  for {set i 0} {\$i < 3} {incr i} { lappend b [format %02x [debug read memory [expr {0xE000+\$i}]]] }
  puts stderr "OUT=RES=[join \$b {}]"
  exit 0
}
TCL
RAW=$(timeout 30 "$OMX" -machine "${FT_MACHINE:-C-BIOS_MSX2}" -script "$TCL" 2>&1 | sed -n 's/.*OUT=//p')
rm -f "$TCL"

RES=$(echo "$RAW" | sed -n 's/^RES=\([0-9a-f]*\).*/\1/p' | head -1)
echo "  RES[] = ${RES:-<none>}"
[ -n "$RES" ] || { echo "  RESULT: FAIL (no output)"; exit 1; }
if [ "${RES:0:2}" = "a5" ]; then echo "  RESULT: PASS  (turnkey far-call w/ banked multiply: play_level(3)=$((0x${RES:4:2}${RES:2:2})))"; exit 0
else echo "  RESULT: FAIL (RES=$RES)"; exit 1; fi
