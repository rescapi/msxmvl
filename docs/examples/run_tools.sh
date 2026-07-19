#!/usr/bin/env bash
# Run every host-side asset-pipeline golden test. These guard the author-time tools
# (tools/img2*.py) against drift: each generates a deterministic image, runs the
# converter, and asserts the emitted C arrays are byte-stable. They need Python +
# Pillow (not the MSX toolchain), so they run in their own CI step, separate from the
# emulator examples in run_all.sh.
set -uo pipefail
H="$(cd "$(dirname "$0")" && pwd)"
fail=0
for t in "$H"/*_test.py; do
	[ -f "$t" ] || continue
	echo "== $(basename "$t")"
	if ! python3 "$t"; then fail=1; fi
done
echo
[ $fail -eq 0 ] && echo "ALL ASSET-PIPELINE TESTS PASS" || echo "SOME ASSET-PIPELINE TESTS FAILED"
exit $fail
