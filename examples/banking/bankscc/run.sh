#!/usr/bin/env bash
# Acceptance test for MAPPER KONAMI_SCC (docs/Code-Banking-Mappers.md): the
# turnkey far-call program built as a Konami-SCC image (window select 0x9000),
# booted with -romtype KonamiSCC on C-BIOS_MSX1 AND C-BIOS_MSX2 — plus the SCC
# hardware quirk asserted as a build check: a code bank on SEG 63 (select
# value low 6 bits all 1 = SCC enable, not ROM) must FAIL the build.
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
carta $W/game.rom -romtype KonamiSCC
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

# 3. negative: a code bank on SEG 63 must be a BUILD ERROR naming the SCC
#    quirk (a thunk mapping seg 63 would call into SCC registers, not code).
#    Built at 512 KB so seg 63 is well inside the image: the refusal is the
#    SCC check, not the ROM-size one.
sed 's/^  6 /  63/' bank.manifest > bank.bad.manifest
if ERR=$(bash "$BP" --build bank.bad.manifest bad.rom 512 2>&1); then
  echo "  SCC-seg-63: FAIL (build was accepted)"; fail=1
elif echo "$ERR" | grep -q "SCC sound chip"; then
  echo "  SCC-seg-63: PASS (build rejected naming the SCC quirk)"
else
  echo "  SCC-seg-63: FAIL (wrong message: $ERR)"; fail=1
fi

[ "$fail" -eq 0 ] && { echo "  RESULT: PASS"; exit 0; } || { echo "  RESULT: FAIL"; exit 1; }
