#!/usr/bin/env bash
# Turnkey banked-ROM build: the developer wrote only plain C (far-callable
# definitions marked FAR_FN) + bank.manifest. ONE bankpack command does the rest:
# scan FAR_FN -> far.h + thunks, compile, link each bank, patch the tables,
# assemble the ROM. Then boot on C-BIOS_MSX2 and assert play_level(3) == 350.
# Also asserts the misuse guard: FAR_FN in a RESIDENT source fails the build.
set -uo pipefail
export SDL_VIDEODRIVER=dummy
DIR=$(cd "$(dirname "$0")" && pwd)
ROOT="$DIR"; while [ ! -f "$ROOT/lib/ext/farrt.asm" ] && [ "$ROOT" != / ]; do ROOT=$(dirname "$ROOT"); done
OMX="${OPENMSX:-$(command -v openmsx || echo /home/remco/tools/openmsx/bin/openmsx)}"
BP="$ROOT/tools/bankpack.sh"
W=$(mktemp -d); trap 'rm -rf "$W"' EXIT

# Stage the program (sources + manifest) in a scratch dir; the build runs there.
# --build auto-locates farrt.asm by searching upward from the manifest -- no repo
# above /tmp, so point BANKPACK_FARRT at it (types.h is staged for the same reason).
cp "$ROOT/lib/gen/types.h" "$DIR/main.c" "$DIR/bankA.c" "$DIR/bankB.c" "$DIR/bank.manifest" "$W/"
cd "$W"
export BANKPACK_FARRT="$ROOT/lib/ext/farrt.asm"

# 1. the whole build is ONE command
bash "$BP" --build bank.manifest game.rom 64 || { echo "BUILD FAIL"; exit 2; }

# 2. negative: FAR_FN in a RESIDENT source must be a clear build error
cp main.c badmain.c
printf '\nFAR_FN u16 oops(u16 x) { return x; }\n' >> badmain.c
sed 's/main\.rel/badmain.rel/' bank.manifest > bank.bad.manifest
if ERR=$(bash "$BP" --build bank.bad.manifest bad.rom 64 2>&1); then
  echo "  FAR_FN-in-resident: FAIL (build was accepted)"; exit 1
fi
if echo "$ERR" | grep -q "resident functions need no FAR_FN"; then
  echo "  FAR_FN-in-resident: PASS (build rejected with a clear message)"
else
  echo "  FAR_FN-in-resident: FAIL (wrong message: $ERR)"; exit 1
fi

# 3. run: boot, break at _mark_end, read the result bytes
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
