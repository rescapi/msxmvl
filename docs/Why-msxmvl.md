# Why msxmvl

There are already good C libraries for the MSX. This one exists because we wanted three things at
the same time that nobody had put together: **speed you can verify**, **documentation you can
trust**, and **a way past the 64 KB wall**.

## Speed you can verify

Most "fast" claims in retro-computing are folklore. Ours are counted.

Every function is benchmarked head-to-head against MSXgl by linking the *same driver* against both
libraries and counting Z80 T-states in a cycle-accurate emulator. The result: **109 of 152 functions
are faster, 41 identical, one is 2.8% slower.** We publish the loss too — see
[Benchmarks](Benchmarks.md).

This is only possible because the MSX2 is an unusually honest machine to optimise for. Every
instruction has a fixed, documented cost; there is no cache and no branch predictor to hide behind.
"Faster" is arithmetic, not vibes.

## Documentation you can trust

**Every code sample in these pages is a real program that was compiled, booted on an emulated MSX,
and checked** — 46 of them. If a sample is shown here, it passed.

And it *stays* true: a drift guard compares every executable line in the docs against the tested
example it came from, and **fails the build** if they diverge. You cannot edit a code sample on
these pages without editing the test that proves it.

That matters more than it sounds. Retro documentation rots quietly — a snippet that was right for a
different screen mode, a register that got renamed, a call that silently stopped working. Here, the
failure mode is a red build, not a confused reader at 2 a.m.

## Past the 64 KB wall

A Z80 sees 64 KB. That is the ceiling every ambitious MSX project eventually hits, and it is where
most libraries stop and hand you the raw hardware.

- **[Code Banking](Code-Banking.md)** — write plain C, list your functions in a manifest, and
  `bankpack` builds a MegaROM: cross-bank calls get a generated trampoline, the bank switches, and
  switches back. A far call costs **169 T-states**; calls *within* a bank cost nothing. We measured
  it. And because the ASCII-8 mapper is *cartridge* hardware, this works on an MSX1 too.
- **[Far Pointers](Far-Pointers.md)** — address megabytes of RAM through the MSX2 memory mapper by
  handle, with a scoped-borrow API that makes the dangerous part (a segment swapped out under your
  feet) hard to write by accident.

Neither is a wrapper around a hardware register. They are the parts we had to *design*, and they are
what the library is actually for.

## Honest about what it isn't

- The compat layer (`lib/gen`) is a **clean-room reimplementation of MSXgl's API**. If you know
  MSXgl, you already know it. Guillaume Blanchard's MSXgl is excellent, and it is the interface
  source and the benchmark we measure against — credit where it is due.
- Some modules need hardware a headless test cannot drive, and we say which, on the front page,
  rather than shipping untested code and hoping.
- It is **MSX1 / MSX2 / MSX2+**, and we list exactly which modules run on which machine — because
  we ran them there. A V9938 command engine cannot work on an MSX1, and we won't pretend otherwise.

## The method, in one line

**Measure everything; test every claim; write down what failed.**

Hunting the MSX1 claim is what uncovered a real byte-dropping bug in bulk VRAM writes. Testing the
DOS module is what uncovered four calling-convention bugs that made file *reading* silently return
garbage. Neither would have been found by a library that documented its intentions instead of its
behaviour.

## Where to start

- [Getting Started](Getting-Started.md) — two packages, one ROM, ten minutes.
- [Benchmarks](Benchmarks.md) — the numbers, including the one we lost.
