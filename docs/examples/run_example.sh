#!/usr/bin/env bash
# Doc-example test harness. Compiles ONE self-contained documentation example (a 16 KB MSX2
# ROM), boots it headless on C-BIOS_MSX2, and verifies the sentinel it writes to
# RAM. Every example shown in the documentation is a real driver that passes through here,
# so the docs cannot drift from the code.
#
# Contract: the example defines `volatile u8 __at(0xE000) R[..]` and, on success,
# stores 0xA5 into R[0]. Extra bytes R[1..] may carry observed values (dumped).
#
# Usage: run_example.sh <example.c> "<mod1.c mod2.c ...>" [expect_r0_hex=a5] [ramlen=8]
# A 16 KB ROM leaves page 2 (0x8000) as mapper RAM, which memseg/farmem need and
# the other modules don't care about — so one harness covers every example.
set -uo pipefail
export SDL_VIDEODRIVER=dummy
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"   # repo root (portable; was hardcoded)
# Our own crt0 (lib/ext/crt0_rom16.asm) + SDCC's makebin. No vendored SDK, no z88dk:
# `apt install sdcc openmsx` is the whole toolchain.
CRT0SRC=$ROOT/lib/ext/crt0_rom16.asm
OMX="${OPENMSX:-$(command -v openmsx || echo /home/remco/tools/openmsx/bin/openmsx)}"
EX="$1"; MODS="${2:-}"; WANT="${3:-a5}"; LEN="${4:-8}"

# Target machine. EX_MACHINE=C-BIOS_MSX1 builds for (and boots on) an MSX1: the
# library selects its MSX1 code paths via MSX_VERSION, which defaults to MSX_2.
MACHINE="${EX_MACHINE:-C-BIOS_MSX2}"
DEFS=""
case "$MACHINE" in
  *MSX1*)   DEFS="-DMSX_VERSION=MSX_1"  ;;
  *MSX2+*)  DEFS="-DMSX_VERSION=MSX_2P" ;;
esac
# EX_DEFS: extra -D flags for this example (e.g. -DSCC_SLOT_MODE=SCC_SLOT_AUTO)
DEFS="$DEFS ${EX_DEFS:-}"

W=$(mktemp -d); trap 'rm -rf "$W"' EXIT
# All headers are made available (a module may include a sibling's header, and
# some modules -- e.g. mutex -- are header-only and need no link object at all).
# Only the .c files named in $MODS are compiled and linked.
cp "$ROOT"/lib/gen/*.h "$W/"
cp "$ROOT"/lib/ext/*.h "$W/" 2>/dev/null || true
# modules may live in lib/ext (extensions) or lib/gen (compat layer); copy the .c
# from whichever directory has it.
for m in $MODS; do
  for d in ext gen; do
    if [ -f "$ROOT/lib/$d/$m" ]; then
      cp "$ROOT/lib/$d/$m" "$W/"
      h="${m%.c}.h"; [ -f "$ROOT/lib/$d/$h" ] && cp "$ROOT/lib/$d/$h" "$W/"
      break
    fi
  done
done
cp "$EX" "$W/ex.c"
cp "$CRT0SRC" "$W/crt0.asm"

( cd "$W"; export DEFS="$DEFS"; rm -f out.ihx out.rom
  sdasz80 -o crt0.rel crt0.asm 2>build.log || { echo "CRT0 ASM FAIL"; cat build.log; exit 1; }
  RELS=""
  for m in $MODS; do sdcc -c -mz80 --sdcccall 1 $DEFS -I. "$m" 2>>build.log || { echo "MODBUILD FAIL $m"; cat build.log; exit 1; }; RELS="$RELS ${m%.c}.rel"; done
  sdcc -c -mz80 --sdcccall 1 $DEFS -I. ex.c 2>>build.log || { echo "EXBUILD FAIL"; sed -n 's/^ex.c/EXAMPLE/p;/error/p' build.log; exit 1; }

  # crt0.rel FIRST: the cartridge header must land exactly at 0x4000. The crt0
  # declares the area order, so _INITIALIZER lands in ROM after the code and the
  # startup copy actually has something to copy.
  sdcc -o out.ihx --code-loc 0x4000 --data-loc 0xC000 -mz80 --no-std-crt0 --sdcccall 1 \
       crt0.rel ex.rel $RELS 2>>build.log || { echo "LINK FAIL"; cat build.log; exit 1; }

  IS=$(awk '$2=="s__INITIALIZER"{print $1}' out.map); IL=$(awk '$2=="l__INITIALIZER"{print $1}' out.map)
  [ "$((0x$IS))" -ge $((0x4000)) ] && [ "$((0x$IS + 0x$IL))" -le $((0x8000)) ] \
    || { echo "INIT PLACEMENT FAIL (_INITIALIZER at 0x$IS, not in ROM)"; exit 1; }

  # makebin (ships with SDCC) replaces z88dk's hex2bin: build 0x0000-0x7FFF, then
  # drop the first 16 KB so the image starts at the cartridge's 0x4000.
  makebin -s 32768 out.ihx full.bin 2>>build.log || { echo "MAKEBIN FAIL"; cat build.log; exit 1; }
  dd if=full.bin of=out.rom bs=16384 skip=1 count=1 2>/dev/null
  ) || { echo "BUILD ERROR"; exit 2; }

ROM="$W/out.rom"; MAP="$W/out.map"
[ -f "$ROM" ] || { echo "NO ROM"; exit 2; }
# break at main's terminal spin: the example ends in for(;;){}. Use _mark_end if present,
# else fall back to a fixed emulated-time sample.
MEND=$(awk '$2=="_mark_end"{print "0x"substr($1,length($1)-3)}' "$MAP")

# Optional VDP-register check: EX_VREG_ASSERT="2=7f" asserts VDP R#2 == 0x7f.
VREG=""; VWANT=""
if [ -n "${EX_VREG_ASSERT:-}" ]; then VREG="${EX_VREG_ASSERT%%=*}"; VWANT="${EX_VREG_ASSERT##*=}"; fi

# Inject a key AFTER the program is already polling, so there is a clean up->down
# edge for edge-detecting reads (Keyboard_IsKeyPushed). Built here (not inline in
# the heredoc) to avoid nested-brace confusion in ${var:+...} when unset.
KEYLINE=""
if [ -n "${EX_KEYDOWN:-}" ]; then KEYLINE="after realtime 1 { keymatrixdown $EX_KEYDOWN }"; fi

# EX_DBG="<debuggable name>" [EX_DBGOFS] [EX_DBGLEN] dumps a device's registers
# straight from the emulated chip -- used to verify write-only hardware (SCC, OPLL)
# that the CPU cannot read back.
DBG="${EX_DBG:-}"; DBGOFS="${EX_DBGOFS:-0}"; DBGLEN="${EX_DBGLEN:-8}"; DBGWANT="${EX_DBGWANT:-}"

# EX_EXT=<name> plugs in an openMSX extension (e.g. scc). It is inserted from TCL
# AFTER `carta`, NOT via a -ext command-line flag: openMSX fills slots in the order
# devices appear, so a command-line -ext would take slot 1 and push our ROM to slot
# 2 -- the reverse of what the scc module expects (cart 1, SCC in slot 2).
EXTLINE=""
if [ -n "${EX_EXT:-}" ]; then EXTLINE="ext ${EX_EXT}"; fi

TCL=$(mktemp)
cat > "$TCL" <<TCL
set renderer none
set throttle off
after realtime 25 { puts stderr "OUT=TIMEOUT PC=[format %04x [reg PC]]"; exit 1 }
carta $ROM
$EXTLINE
$KEYLINE
proc dump {} {
  set b {}
  for {set i 0} {\$i < $LEN} {incr i} { lappend b [format %02x [debug read memory [expr {0xE000+\$i}]]] }
  puts stderr "OUT=R=[join \$b {}]"
  if {"$VREG" ne ""} { puts stderr "OUT=VREG=[format %02x [debug read {VDP regs} $VREG]]" }
  if {"$DBG" ne ""} {
    set d {}
    for {set i 0} {\$i < $DBGLEN} {incr i} { lappend d [format %02x [debug read {$DBG} [expr {$DBGOFS+\$i}]]] }
    puts stderr "OUT=DBG=[join \$d {}]"
  }
  exit 0
}
TCL
if [ -n "$MEND" ]; then echo "debug set_bp $MEND {} { dump }" >> "$TCL"
else echo "after realtime 4 { dump }" >> "$TCL"; fi

# EX_DISK=<file.dsk> inserts a disk (needed by the raw-sector examples, which call
# into the machine's disk-interface ROM).
DISKARG=""
if [ -n "${EX_DISK:-}" ]; then DISKARG="-diska $EX_DISK"; fi
ALL=$(timeout 60 "$OMX" -machine "$MACHINE" $DISKARG -script "$TCL" 2>&1 | sed -n 's/.*OUT=//p')
rm -f "$TCL"
RB=$(echo "$ALL" | sed -n 's/^R=\([0-9a-f]*\).*/\1/p' | head -1)
VG=$(echo "$ALL" | sed -n 's/^VREG=\([0-9a-f]*\).*/\1/p' | head -1)
DG=$(echo "$ALL" | sed -n 's/^DBG=\([0-9a-f]*\).*/\1/p' | head -1)
name=$(basename "$EX")
if [ -z "$RB" ]; then echo "  $name: FAIL (no output: $ALL)"; exit 1; fi
got="${RB:0:2}"
ok=1
[ "${got,,}" = "${WANT,,}" ] || ok=0
if [ -n "$VREG" ]; then [ "${VG,,}" = "${VWANT,,}" ] || ok=0; fi
# EX_DBGWANT: assert the emulated chip's own registers. Needed for write-only hardware
# (OPLL) that the CPU physically cannot read back.
if [ -n "$DBGWANT" ]; then [ "${DG,,}" = "${DBGWANT,,}" ] || ok=0; fi
if [ "$ok" = 1 ]; then echo "  $name: PASS  R[]=$RB${VREG:+  R#$VREG=$VG}${DG:+  DBG=$DG}"; exit 0
else echo "  $name: FAIL  R[0]=$got want $WANT${VREG:+  R#$VREG=$VG want $VWANT}  (R[]=$RB)${DG:+  DBG=$DG${DBGWANT:+ want $DBGWANT}}"; exit 1; fi
