# Attribution

msxmvl is an original, clean-room MSX C library. Where a module's public interface (function
signatures, struct layouts, documented behaviour, and register/BDOS ABIs) is shared with an
existing library, the source header says so — e.g. "Derived from MSXgl engine/src/vdp.h
(interface only)". Those references are to the PUBLIC interface of:

  MSXgl — Guillaume "Aoineko" Blanchard — https://github.com/aoineko-fr/MSXgl  (CC BY-SA 4.0)

No MSXgl (or other third-party) implementation source was copied into msxmvl. Interfaces/ABIs
are facts and are not copyrightable expression; the implementations here are independently
written. See each module header for its specific provenance note.

## MoonBlaster

`lib/ext/moonblaster_player.asm` is the MoonBlaster 1.4 replayer (`mbplay`, Moonsoft) — the
original work of this library's author, optimised for interrupt time and relicensed here under
the repository's MIT license. The bundled example song `docs/examples/assets/EXAMP1.MBM`
("Example Song") and its sample kit `EXAMPLE.MBK` are by C.v/d Geest, from the original
MoonBlaster 1.4 disk, included with permission.
