#!/usr/bin/env bash
# DOS example harness. Builds ONE documentation example as an MSX-DOS .COM, boots it on a
# real MSX-DOS 2.20 disk in openMSX, breaks at _mark_end and reads the sentinel.
#
# Same contract as the ROM harness, with one difference: DOS examples put their
# result at 0xB000, not 0xE000 -- under MSX-DOS 2 the kernel lives high in RAM,
# so 0xB000 is a safe spot inside the TPA.
#
# Usage: run_example_dos.sh <example.c> "<mod1.c ...>" [expect_r0=a5] [ramlen=8]
#
# REQUIRES an MSX-DOS system disk and a machine with a disk ROM. Both contain
# copyrighted system software and are NOT redistributable -- so this harness
# cannot run in public CI. The ROM-based examples need none of it.
set -uo pipefail
export SDL_VIDEODRIVER=dummy
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"   # repo root (portable; was hardcoded)
CRT0SRC=$ROOT/lib/ext/crt0_dos.asm     # our own crt0; no vendored SDK, no z88dk
OMX="${OPENMSX:-$(command -v openmsx || echo /home/remco/tools/openmsx/bin/openmsx)}"
MACHINE=Philips_NMS_8245-16

# EX_DOS=1 -> MSX-DOS 1 disk, no DOS2 kernel. EX_DOS=2 (default) -> MSX-DOS 2.
# The FCB API is MSX-DOS 1 (and still works under DOS 2); the file-HANDLE API is
# MSX-DOS 2 only and will fail on a DOS 1 disk. That distinction is the point of
# having both here.
DOSVER="${EX_DOS:-2}"
if [ "$DOSVER" = "1" ]; then
  BOOTDSK=$ROOT/harness/dos/msxdos1.dsk; DOSEXT=""
else
  BOOTDSK=$ROOT/harness/dos/msxdos2.dsk; DOSEXT="-ext msxdos2"
fi

EX="$1"; MODS="${2:-}"; WANT="${3:-a5}"; LEN="${4:-8}"
name=$(basename "$EX")
[ -f "$BOOTDSK" ] || { echo "  $name: SKIP (no MSX-DOS disk at $BOOTDSK)"; exit 0; }

W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
cp "$ROOT"/lib/gen/*.h "$W/"
cp "$ROOT"/lib/ext/*.h "$W/" 2>/dev/null || true
cp "$(dirname "$EX")"/*.h "$W/" 2>/dev/null || true
for m in $MODS; do
  for d in ext gen; do
    [ -f "$ROOT/lib/$d/$m" ] && { cp "$ROOT/lib/$d/$m" "$W/"; break; }
  done
done
cp "$EX" "$W/ex.c"
cp "$CRT0SRC" "$W/crt0.asm"

# 1. build the .COM (TPA starts at 0x0100)
( cd "$W"
  sdasz80 -o crt0.rel crt0.asm 2>build.log || { echo "CRT0 ASM FAIL"; cat build.log; exit 1; }
  RELS=""
  for m in $MODS; do
    sdcc -c -mz80 --sdcccall 1 --code-loc 0x0100 -I. "$m" 2>>build.log || { echo "MODBUILD FAIL $m"; cat build.log; exit 1; }
    RELS="$RELS ${m%.c}.rel"
  done
  sdcc -c -mz80 --sdcccall 1 --code-loc 0x0100 -I. ex.c 2>>build.log || { echo "EXBUILD FAIL"; grep -i error build.log; exit 1; }
  sdcc -o d.ihx --code-loc 0x0100 --data-loc 0 -mz80 --no-std-crt0 --sdcccall 1 \
       crt0.rel ex.rel $RELS 2>>build.log || { echo "LINK FAIL"; cat build.log; exit 1; }
  # makebin (from SDCC) instead of z88dk's hex2bin; drop the 0x100 TPA prefix.
  makebin -p -s 65536 d.ihx full.bin 2>>build.log || { echo "MAKEBIN FAIL"; cat build.log; exit 1; }
  dd if=full.bin of=d.com bs=1 skip=256 2>/dev/null
  ) || { echo "  $name: BUILD ERROR"; exit 2; }
[ -f "$W/d.com" ] || { echo "  $name: NO .COM"; exit 2; }

# 2. put the .COM on a copy of the boot disk and autoexec it
cp "$BOOTDSK" "$W/run.dsk"; cp "$W/d.com" "$W/T.COM"
printf 'a:\r\nt\r\n' > "$W/AUTOEXEC.BAT"  # bare name: MSX-DOS 1's COMMAND.COM does not take the .COM extension
IMP=$(mktemp)
printf 'set renderer none\ndiska %s/run.dsk\ncatch {diskmanipulator delete diska autoexec.bat}\ndiskmanipulator import diska %s/T.COM %s/AUTOEXEC.BAT\nexit 0\n' "$W" "$W" "$W" > "$IMP"
timeout 20 "$OMX" -machine "$MACHINE" -script "$IMP" >/dev/null 2>&1; rm -f "$IMP"

# 3. boot MSX-DOS, let AUTOEXEC.BAT run T.COM, then sample RAM.
#
# NOT a breakpoint on _mark_end: MSX-DOS loads COMMAND.COM into the SAME TPA at
# 0x0100, so a bp on the example's address fires while COMMAND.COM is still
# running -- long before our .COM is even loaded -- and dumps empty RAM. Instead
# give DOS time to boot and run the program (which then spins in _mark_end
# forever, so the result stays put) and sample at a fixed emulated time.
TCL=$(mktemp)
cat > "$TCL" <<TCL
set renderer none
set scheduler_mode fast
set throttle off
after realtime 90 { puts stderr "OUT=TIMEOUT"; exit 1 }
after time ${BW2:-16.0} {
  set b {}
  for {set i 0} {\$i < $LEN} {incr i} { lappend b [format %02x [debug read memory [expr {0xB000+\$i}]]] }
  puts stderr "OUT=R=[join \$b {}]"
  exit 0
}
TCL
RB=$(timeout 100 "$OMX" -machine "$MACHINE" $DOSEXT ${EX_EXT:+-ext $EX_EXT} -diska "$W/run.dsk" -script "$TCL" 2>&1 | sed -n 's/.*OUT=R=//p' | head -1)
rm -f "$TCL"

[ -n "$RB" ] || { echo "  $name: FAIL (no output — .COM never reached _mark_end)"; exit 1; }
got="${RB:0:2}"
if [ "${got,,}" = "${WANT,,}" ]; then echo "  $name: PASS  R[]=$RB"; exit 0
else echo "  $name: FAIL  R[0]=$got want $WANT  (R[]=$RB)"; exit 1; fi
