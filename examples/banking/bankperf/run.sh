#!/usr/bin/env bash
# Measure real far-call cost (T-states) via three PC-marker intervals in one boot:
#   near baseline (_m0.._m1), single far-call (_m1.._m2), nested (_m2.._m3).
# Far overhead = single - near; nested extra = nested - single.
set -uo pipefail
export SDL_VIDEODRIVER=dummy
DIR=$(cd "$(dirname "$0")" && pwd)
ROOT="$DIR"; while [ ! -f "$ROOT/lib/ext/farrt.asm" ] && [ "$ROOT" != / ]; do ROOT=$(dirname "$ROOT"); done
OMX="${OPENMSX:-$(command -v openmsx || echo /home/remco/tools/openmsx/bin/openmsx)}"
BP="$ROOT/tools/bankpack.sh"
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT

cp "$ROOT/lib/gen/types.h" "$ROOT/lib/ext/farrt.asm" "$DIR"/{main.c,bankA.c,bankB.c,bank.manifest} "$W/"
cd "$W"
bash "$BP" --gen bank.manifest . >/dev/null
sdasz80 -o farrt.rel farrt.asm; sdasz80 -o _bp_far_thunks.rel _bp_far_thunks.asm
for c in main bankA bankB; do sdcc -c -mz80 --sdcccall 1 -I. -o "$c.rel" "$c.c" || { echo "CC $c FAIL"; exit 1; }; done
bash "$BP" bank.manifest game.rom 64 >/dev/null || { echo "BANKPACK FAIL"; exit 2; }

# marker addresses from the resident .noi
declare -A M
for i in 0 1 2 3; do M[$i]=$(awk -v s="_m$i" '$1=="DEF" && $2==s{print $3}' _bp_resident.noi); done

TCL=$(mktemp)
cat > "$TCL" <<TCL
set renderer none
set scheduler_mode fast
set throttle off
set freq [machine_info z80_freq]
after realtime 25 { puts stderr "OUT=TIMEOUT"; exit 1 }
carta $W/game.rom -romtype ASCII8
debug set_bp ${M[0]} {} { set ::t0 [machine_info time] }
debug set_bp ${M[1]} {} { set ::t1 [machine_info time] }
debug set_bp ${M[2]} {} { set ::t2 [machine_info time] }
debug set_bp ${M[3]} {} {
  set t3 [machine_info time]
  set n  [expr {round((\$::t1-\$::t0)*\$freq)}]
  set f  [expr {round((\$::t2-\$::t1)*\$freq)}]
  set ne [expr {round((\$t3-\$::t2)*\$freq)}]
  puts stderr "OUT=NEAR=\$n FAR=\$f NESTED=\$ne"
  exit 0
}
TCL
RAW=$(timeout 30 "$OMX" -machine C-BIOS_MSX2 -script "$TCL" 2>&1 | sed -n 's/.*OUT=//p' | head -1)
rm -f "$TCL"
echo "  raw: $RAW"
n=$(echo "$RAW"  | sed -n 's/.*NEAR=\([0-9]*\).*/\1/p')
f=$(echo "$RAW"  | sed -n 's/.*FAR=\([0-9]*\).*/\1/p')
ne=$(echo "$RAW" | sed -n 's/.*NESTED=\([0-9]*\).*/\1/p')
[ -n "$n" ] && [ -n "$f" ] && [ -n "$ne" ] || { echo "  MEASURE FAIL"; exit 1; }
echo "  near call (baseline)      : $n T-states"
echo "  single far-call           : $f T-states  (overhead vs near: $((f-n)))"
echo "  nested far-call (2 levels) : $ne T-states  (2nd level adds: $((ne-f)))"
echo
echo "  => far-call overhead ~ $((f-n)) T-states/call (@3.58MHz = $(awk "BEGIN{printf \"%.1f\", (($f-$n))/3.58}") us);"
echo "     a 60Hz frame is ~59736 T-states, so ~$(( 59736/(f-n) )) far-calls fill one frame."
