#!/usr/bin/env python3
# gentables_test.py -- golden/drift test for the maths-table generator
# (tools/gentables.py). No dependencies beyond the stdlib. Asserts the table shapes,
# a few known values (sin/cos quarter points, atan endpoints), and byte-stable golden
# hashes of the emitted arrays.
import os, subprocess, sys, tempfile, hashlib, re

HERE = os.path.dirname(os.path.abspath(__file__))
TOOL = os.path.join(HERE, "..", "..", "tools", "gentables.py")

def run(d, kind, name, *extra):
    r = subprocess.run([sys.executable, TOOL, kind, name, *map(str, extra)],
                       capture_output=True, text=True, cwd=d)
    if r.returncode != 0:
        print(f"FAIL: {kind} errored\n", r.stderr); sys.exit(1)
    return open(os.path.join(d, name + "_table.h")).read()

def values(text, name):
    m = re.search(r"\b" + re.escape(name) + r"\[\d+\]=\{([^}]*)\}", text)
    return [int(x) for x in m.group(1).split(",")]

def golden(text):
    return hashlib.sha256("\n".join(l for l in text.splitlines()
                                    if l.startswith("const")).encode()).hexdigest()[:16]

def main():
    with tempfile.TemporaryDirectory() as d:
        # sine, 256 entries, amp 127 (i8)
        ts = run(d, "sin", "s")
        s = values(ts, "s")
        if len(s) != 256: print("FAIL: sin length", len(s)); sys.exit(1)
        if "const i8 s" not in ts: print("FAIL: sin not i8\n", ts); sys.exit(1)
        if (s[0], s[64], s[128], s[192]) != (0, 127, 0, -127):
            print("FAIL: sin quarter points", s[0], s[64], s[128], s[192]); sys.exit(1)

        # cosine = sine shifted a quarter: cos[0]=amp, cos[64]=0
        tc = run(d, "cos", "c")
        c = values(tc, "c")
        if (c[0], c[64]) != (127, 0):
            print("FAIL: cos quarter points", c[0], c[64]); sys.exit(1)

        # atan: slope 0 -> 0 brads; monotonic non-decreasing; last < 32 (i.e. <45deg)
        ta = run(d, "atan", "a")
        a = values(ta, "a")
        if "const u8 a" not in ta: print("FAIL: atan not u8\n", ta); sys.exit(1)
        if a[0] != 0 or a[-1] > 32 or any(a[i] > a[i+1] for i in range(len(a)-1)):
            print("FAIL: atan shape", a[0], a[-1]); sys.exit(1)

        # i16 path: amp 256 -> i16, sin[64]=256
        t16 = run(d, "sin", "big", 256, 256)
        if "const i16 big" not in t16 or values(t16, "big")[64] != 256:
            print("FAIL: i16 amplitude path\n", t16); sys.exit(1)

        for label, text, want in (("sin", ts, "8887dbc8af1754a8"),
                                  ("cos", tc, "a44c87a6820c8051"),
                                  ("atan", ta, "a7962b1cdf7c74c8")):
            g = golden(text)
            if g != want:
                print(f"FAIL: {label} golden drift (got {g}, want {want})"); sys.exit(1)

        print("gentables_test PASS: sin/cos/atan shapes + known values + arrays byte-stable")

if __name__ == "__main__":
    main()
