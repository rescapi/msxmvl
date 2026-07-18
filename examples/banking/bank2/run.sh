#!/usr/bin/env bash
# Build the 2-bank ASCII-8 MegaROM proof with bankpack, boot it headless on
# C-BIOS_MSX2, and assert the far-call self-check bytes at RAM 0xE000.
#
# Proves: (1) the window switches to the banked segment    RES[2]=0xB4
#         (2) code in the banked segment actually executes  RES[3]=0x5A
#         (3) the previous segment is restored afterwards    RES[4]=0x41
#         (4) the banked segment can call a resident routine RES[5]=0x3C
#             (bankpack injects the resident symbol's address)
#         => RES[0]=0xA5 overall PASS.
set -uo pipefail
export SDL_VIDEODRIVER=dummy
DIR=$(cd "$(dirname "$0")" && pwd)
ROOT="$DIR"; while [ ! -f "$ROOT/lib/ext/farrt.asm" ] && [ "$ROOT" != / ]; do ROOT=$(dirname "$ROOT"); done
OMX="${OPENMSX:-$(command -v openmsx || echo /home/remco/tools/openmsx/bin/openmsx)}"
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT

# --- assemble each bank to .rel, then let bankpack link + assemble the ROM ---
sdasz80 -o "$W/resident.rel" "$DIR/resident.asm" || { echo "ASM resident FAIL"; exit 1; }
sdasz80 -o "$W/banked.rel"   "$DIR/banked.asm"   || { echo "ASM banked FAIL"; exit 1; }
cp "$DIR/bank.manifest" "$W/"
( cd "$W" && bash "$ROOT/tools/bankpack.sh" bank.manifest game.rom 64 ) || { echo "BANKPACK FAIL"; exit 2; }

ROM="$W/game.rom"
MEND=$(awk '$1=="DEF" && $2=="_mark_end"{print $3}' "$W/_bp_resident.noi")
[ -n "$MEND" ] || { echo "no _mark_end in resident .noi"; exit 2; }
echo "  _mark_end @ $MEND"

# --- boot as an ASCII-8 cartridge, break at _mark_end, dump RES[0..7] ---
TCL=$(mktemp)
cat > "$TCL" <<TCL
set renderer none
set scheduler_mode fast
set throttle off
after realtime 25 { puts stderr "OUT=TIMEOUT"; exit 1 }
carta $ROM -romtype ASCII8
debug set_bp $MEND {} {
  set b {}
  for {set i 0} {\$i < 8} {incr i} { lappend b [format %02x [debug read memory [expr {0xE000+\$i}]]] }
  puts stderr "OUT=RES=[join \$b {}]"
  exit 0
}
TCL
RAW=$(timeout 30 "$OMX" -machine C-BIOS_MSX2 -script "$TCL" 2>&1 | sed -n 's/.*OUT=//p')
rm -f "$TCL"

RES=$(echo "$RAW" | sed -n 's/^RES=\([0-9a-f]*\).*/\1/p' | head -1)
echo "  RES[] = ${RES:-<none>}"
[ -n "$RES" ] || { echo "  RESULT: FAIL (no output)"; exit 1; }
r0=${RES:0:2}; r1=${RES:2:2}; r2=${RES:4:2}; r3=${RES:6:2}; r4=${RES:8:2}; r5=${RES:10:2}
echo "    RES[0] verdict  = $r0 (want a5)"
echo "    RES[1] before   = $r1 (want 41)"
echo "    RES[2] mapped   = $r2 (want b4)"
echo "    RES[3] banked   = $r3 (want 5a)"
echo "    RES[4] restored = $r4 (want 41)"
echo "    RES[5] bank->res = $r5 (want 3c)"
if [ "$r0" = "a5" ]; then echo "  RESULT: PASS"; exit 0
else echo "  RESULT: FAIL"; exit 1; fi
