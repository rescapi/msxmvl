#!/usr/bin/env bash
# Boot-disk example harness  ***LOCAL-ONLY — NOT part of run_all.sh / public CI***
#
# Builds a documentation example as a raw TPA payload, packs it (plus, for the
# mini-diskOS example, a data blob) into a self-booting FAT12 .dsk with
# tools/mkbootdsk.sh, boots THAT DISK DIRECTLY on a real disk-ROM machine
# (Philips NMS 8245) with NO MSX-DOS and NO AUTOEXEC, then reads the sentinel at
# 0xB000. Same result contract as run_example_dos.sh; the difference is that here
# nothing copyrighted is on the disk — our boot sector loads the payload itself.
#
# Why local-only: the 8245 has a built-in disk ROM (copyrighted, not
# redistributable), and C-BIOS — the only ROM public CI may ship — has no disk ROM
# and no floppy controller, so a .dsk cannot even be inserted. This is exactly the
# run_example_dos.sh precedent. The boot IS testable headless locally, and this
# script is the proof.
#
# Usage: run_example_bootdisk.sh <example.c> "<mod1.c ...>" [expect_r0=a5] [ramlen=8]
set -uo pipefail
export SDL_VIDEODRIVER=dummy
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
CRT0SRC=$ROOT/lib/ext/crt0_bootdisk.asm
MKBOOT=$ROOT/tools/mkbootdsk.sh
OMX="${OPENMSX:-$(command -v openmsx || echo /home/remco/tools/openmsx/bin/openmsx)}"
MACHINE=Philips_NMS_8245-16

EX="$1"; MODS="${2:-}"; WANT="${3:-a5}"; LEN="${4:-8}"
name=$(basename "$EX")

W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
cp "$ROOT"/lib/gen/*.h "$W/"
cp "$ROOT"/lib/ext/*.h "$W/" 2>/dev/null || true
for m in $MODS; do
  for d in ext gen; do
    [ -f "$ROOT/lib/$d/$m" ] && { cp "$ROOT/lib/$d/$m" "$W/"; break; }
  done
done
cp "$EX" "$W/ex.c"
cp "$CRT0SRC" "$W/crt0.asm"

# 1. build the raw TPA payload (identical model to a .COM: image from 0x0100)
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
  makebin -p -s 65536 d.ihx full.bin 2>>build.log || { echo "MAKEBIN FAIL"; cat build.log; exit 1; }
  dd if=full.bin of=PAYLOAD.BIN bs=1 skip=256 2>/dev/null
  ) || { echo "  $name: BUILD ERROR"; exit 2; }
[ -f "$W/PAYLOAD.BIN" ] || { echo "  $name: NO PAYLOAD"; exit 2; }

# 2. write the manifest; if the example uses diskos, add the data blob it reads.
{ echo "PAYLOAD.BIN  load  0x0100"; } > "$W/manifest.txt"
case " $MODS " in
  *" diskos.c "*)
    # blob id 1: 260 bytes (crosses no sector but is a real length), known pattern
    { printf '\xc0\xde\x01\x02'; head -c 256 /dev/zero; } > "$W/BLOB.DAT"
    echo "BLOB.DAT     resident" >> "$W/manifest.txt" ;;
esac

# 3. pack the self-booting disk (pure bash FAT12; no DOS on it)
bash "$MKBOOT" "$W/manifest.txt" "$W/run.dsk" 720 > "$W/mkboot.log" 2>&1 \
  || { echo "  $name: MKBOOTDSK FAIL"; cat "$W/mkboot.log"; exit 2; }

# 4. boot the disk directly (no -ext, no DOS) and sample RAM at 0xB000.
#    No COMMAND.COM here, so 0x0100 is OUR payload from the first instant — but a
#    fixed emulated-time sample (as in the DOS harness) is simplest and robust.
TCL=$(mktemp)
cat > "$TCL" <<TCL
set renderer none
set scheduler_mode fast
set throttle off
after realtime 90 { puts stderr "OUT=TIMEOUT"; exit 1 }
after time ${BW:-14.0} {
  set b {}
  for {set i 0} {\$i < $LEN} {incr i} { lappend b [format %02x [debug read memory [expr {0xB000+\$i}]]] }
  puts stderr "OUT=R=[join \$b {}]"
  exit 0
}
TCL
RB=$(timeout 100 "$OMX" -machine "$MACHINE" -diska "$W/run.dsk" -script "$TCL" 2>&1 | sed -n 's/.*OUT=R=//p' | head -1)
rm -f "$TCL"

[ -n "$RB" ] || { echo "  $name: FAIL (no output — payload never reached _mark_end)"; exit 1; }
got="${RB:0:2}"
if [ "${got,,}" = "${WANT,,}" ]; then echo "  $name: PASS  R[]=$RB"; exit 0
else echo "  $name: FAIL  R[0]=$got want $WANT  (R[]=$RB)"; exit 1; fi
