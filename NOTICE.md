# Attribution

msxmvl is an original, clean-room MSX C library. Where a module's public interface (function
signatures, struct layouts, documented behaviour, and register/BDOS ABIs) is shared with an
existing library, the source header says so — e.g. "Derived from MSXgl engine/src/vdp.h
(interface only)". Those references are to the PUBLIC interface of:

  MSXgl — Guillaume "Aoineko" Blanchard — https://github.com/aoineko-fr/MSXgl  (CC BY-SA 4.0)

With one explicitly-marked exception (`lib/ext/unlzsa2.c`, see below), no MSXgl or other
third-party *implementation* source was copied into msxmvl. Interfaces/ABIs are facts and are
not copyrightable expression; the implementations here are independently written. See each
module header for its specific provenance note.

## LZSA2 decompressor (third-party, zlib licence)

`lib/ext/unlzsa2.c` is **not** clean-room. It is the size-optimized LZSA2 Z80 decompressor by
**spke (introspec)** with optimizations by **uniabis**, from Emmanuel Marty's LZSA project
(https://github.com/emmanuel-marty/lzsa). It was transliterated to `sdasz80` syntax and wrapped
as an SDCC naked function — an *altered version* — and one adaptation was made (the match offset
is held in a RAM global instead of `IX`, so SDCC's frame pointer is preserved). The LZSA2
compression algorithms are © 2019 Emmanuel Marty. Used under the **zlib licence**; the full
licence notice and attribution are retained verbatim in the module's source header, and the
alteration is plainly marked, per the licence's terms.

## ayFX sound-effects replayer (third-party, MIT licence)

`lib/ext/ayfx.c` / `ayfx.h` is **not** clean-room. It is SapphiRe's ayFX REPLAYER (fixed-volume
v1.31) as adapted to SDCC by **mvac7** (fR3eL project, `ayFXplayer` v1.1), from
<http://www.z80st.es/blog/tags/ayfx>. It is used under the **MIT licence** with attribution, and
is an *altered version*: it shares msxmvl's PSG shadow (`g_PSG_Regs`) instead of a private buffer,
converts the mixer to msxmvl's active-high convention, drops the buffer-clear in setup so music is
not silenced, and pins its arg-taking `__naked` functions to `__sdcccall(0)`. The alterations are
plainly marked in the module header, per the licence.

## MoonBlaster

`lib/ext/moonblaster_player.asm` is the MoonBlaster 1.4 replayer (`mbplay`, Moonsoft) — the
original work of this library's author, optimised for interrupt time and relicensed here under
the repository's MIT license. The bundled example song `docs/examples/assets/EXAMP1.MBM`
("Example Song") and its sample kit `EXAMPLE.MBK` are by C.v/d Geest, from the original
MoonBlaster 1.4 disk, included with permission.

`lib/ext/moonblaster_wave_player.asm` is the MoonBlaster for MoonSound *Wave* (OPL4) replayer
(MBWave 1.01) — likewise the original work of this library's author (R. Schrijvers / Moonsoft),
relicensed here under the MIT license. It was mechanically converted from the author's GEN80
source to pasmo syntax by `experiments/mbwave-irqtime/tools/gen2pasmo.py` (equates + macro
expansion only, no logic change) and is shipped **as-is** (already efficient — see
`experiments/mbwave-irqtime`), not optimised. `.MWM` example/test songs are third-party
(Jer Der's MoonSound Wave collection) and are **not** bundled — the measurement corpus is
fetched locally and git-ignored.
