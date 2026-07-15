# Attribution

msxmvl is an original, clean-room MSX C library. Where a module's public interface (function
signatures, struct layouts, documented behaviour, and register/BDOS ABIs) is shared with an
existing library, the source header says so — e.g. "Derived from MSXgl engine/src/vdp.h
(interface only)". Those references are to the PUBLIC interface of:

  MSXgl — Guillaume "Aoineko" Blanchard — https://github.com/aoineko-fr/MSXgl  (CC BY-SA 4.0)

No MSXgl (or other third-party) implementation source was copied into msxmvl. Interfaces/ABIs
are facts and are not copyrightable expression; the implementations here are independently
written. See each module header for its specific provenance note.
