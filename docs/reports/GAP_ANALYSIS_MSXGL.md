# Report: msxmvl capability gap analysis vs MSXgl

**Status:** ANALYSIS — no code changes. A prioritized, honest read of where msxmvl is
behind MSXgl (and behind what a serious MSX game/demo dev expects), to steer the roadmap.
Facts about MSXgl come from reading its interfaces only (`vendor/msxgl` is CC BY-SA — we
read it for *what it does*, never for *how it says it*). msxmvl's values frame every call:
faster, smaller, more readable, easier to use, claims measured/verified, clean-room.

## Summary verdict

msxmvl wins on the things it chose to fight for — verified speed, non-rotting docs, and a
*designed* way past the 64 KB wall — and matches MSXgl call-for-call across the core engine
(VDP, sprites, sound chips, DOS, input, math, print, banking). Where it is behind is
**breadth of pre-built content plumbing**: music-tracker replayers, general-purpose
decompressors, exotic input devices and mappers, and above all a **host-side asset
pipeline**. The single biggest structural weakness is that MSXgl ships an image/sprite/
music/table converter suite and msxmvl ships none — a real game must convert assets by
hand or with third-party tools.

## Scope note

Recent msxmvl work is *already landed* and is not counted as a gap: full banking (ASCII-8/16,
Konami/SCC, big-ROM, the C data model, `FAR_FN`/`--build` ergonomics), the crt0 family
(rom16/32/48, dos, dos_mapper, bootdisk), IM2 `isr`, SRAM (`sram`/`pac`), self-booting disk
+ mini-diskOS, the interrupt-time MoonBlaster replayer, and the full VDP/draw/print/tile/
scroll/sprite_fx/display/vsync + PSG/SCC/MSX-Music/MSX-Audio + input/dos/disk/clock/compress/
crypt/localize/fsm/mutex/math/string/memory/fixed_point/g3d/memseg/farmem set.

## Capability comparison

| Capability | MSXgl | msxmvl | Gap | Who needs it |
|---|---|---|---|---|
| Core VDP (SCREEN 0-8), draw, tile, scroll, sprite_fx, display | Yes | Yes (measured faster) | none | everyone |
| PSG / SCC / MSX-Music / MSX-Audio chip drivers | Yes | Yes | none | everyone |
| MSX-DOS 1/2, raw sectors, mapper, boot disk | Yes | Yes | none | disk/tool devs |
| Banking (ASCII8/16, Konami/SCC, big-ROM) | Yes | Yes (+ C data model) | none | any >64 KB project |
| Math / fixed-point / string / memory / crypt / localize / fsm | Yes | Yes (clean-room, measured) | none | everyone |
| Keyboard / joystick / mouse input | Yes | Yes | none | games |
| **Asset pipeline (image→tiles/sprites, table gen, packer)** | Yes (MSXtk suite) | **No** | **major** | every game/demo |
| **Music-tracker replayers** (PT3, Arkos AKG/AKM/AKY, WYZ, VGM/lVGM, NDP, MGLV, SCC-trilo) | Yes (~8 formats) | MoonBlaster 1.4 only | **major** | games/demos |
| **PSG sound-effects layer** (ayFX) | Yes | No | moderate | games |
| **General decompressors** (Pletter, ZX0, LZ48, Bitbuster) | Yes | RLEp only | moderate | asset-heavy ROMs |
| Game framework (pawn + menu, sequence, state, bitfield) | Yes | pawn + fsm | moderate | game devs |
| YJK modes (SCREEN 10/11/12), SCREEN 9, G3 mirror | Yes (turnkey) | register toggle only | minor | 2+ demos |
| Exotic input (paddle, light gun, Mega-drive pad, Ninja Tap, JSX, HID) | Yes | No | minor | niche games |
| Exotic ROM targets/mappers (NEO-8/16, Yamanooto, ASCII16-X, Popolon-SCC, 64K) | Yes | mainstream only | minor | niche publishers |
| Tape/cassette + BASIC-binary targets | Yes | No | minor | hobby/legacy |
| Nextor storage target | Yes | DOS2 + raw only | minor | modern-storage devs |
| PCM sample encode/playback | Yes (pcmenc/pcmplay) | No | minor | speech/FX demos |
| Network / UNAPI TCP (ObsoNET) | Yes | No | minor | networked apps |
| QR code, SJIS/Kanji, printer | Yes | No | minor | niche |
| V9990 (GFX9000) | Deep (~1450-line iface) | Thin subset (untested) | minor | GFX9000 owners |
| **Verified/measured performance claims** | No | **Yes** | msxmvl ahead | quality-driven devs |
| **Drift-guarded, boot-tested docs** | Partial | **Yes** | msxmvl ahead | learners |
| **Clean-room MIT-style license** | CC BY-SA | **permissive** | msxmvl ahead | commercial shippers |

## The biggest gaps

### 1. Host-side asset pipeline — MSXtk (severity: major, effort: L, fit: strong but heavy)
MSXgl ships a C++ tool suite (`tools/MSXtk`): **MSXimg** converts BMP/PNG/GIF into tilesets,
name tables, and 8×8/16×16 sprite banks with per-layer colour extraction; **MSXmath**
generates sin/cos/tan/asin fixed-point tables; **MSXzip** packs data (RLEp, lVGM); **MSXbin**/
**MSXhex** turn binaries into C/asm arrays; plus MSXcrypt and a QR encoder. msxmvl has
`bankpack`, `mkbootdsk`, `mb14blob`, `make_public` — code/disk plumbing, no *content*
plumbing. This is the structural difference: with msxmvl you can *write* a game but you must
source your own converters for every pixel and note of data. It fits the values (a converter
is offline, testable, and can be pure Python/bash rather than a FreeImage-linked C++ build),
but it is a large surface. Recommendation is to scope it hard (see below), not clone MSXtk.

### 2. Music-tracker replayers (severity: major, effort: M per format, fit: strong)
MSXgl bundles replayers for the formats MSX musicians actually use: **PT3** (Vortex/
ProTracker 3), **Arkos Tracker** (AKG/AKM/AKY, incl. 6-channel and Darky), **WYZTracker**,
**VGM/lVGM** logs, **NDP**, **MGLV**, and an SCC replayer (`trilo`). msxmvl plays exactly one
tracker format — MoonBlaster 1.4 — plus the raw chip drivers. For a game or demo, the tracker
choice usually dictates the library, so this is the clearest feature deficit. The good news:
it fits msxmvl's method perfectly — a replayer has a fixed interrupt-time budget, so each one
can be added the way MoonBlaster was, with a *measured* worst-case T-state cost as the
headline claim (a claim MSXgl does not make). Add one widely-used format first (PT3 or Arkos).

### 3. General decompressors (severity: moderate, effort: S-M, fit: strong)
MSXgl provides runtime unpackers for **Pletter, ZX0, LZ48, and Bitbuster**; msxmvl unpacks
only **RLEp**. RLEp is fine for cleared tile maps but poor for graphics/level data, so an
asset-heavy ROM on msxmvl is stuck with weaker compression or a hand-rolled unpacker. A ZX0
or Pletter unpacker is a bounded, pure-logic, oracle-verifiable module — exactly the shape
msxmvl does best, and a natural place to land another measured "faster/smaller" win. This is
the highest-leverage single addition relative to its effort.

### 4. Game framework depth (severity: moderate, effort: S-M, fit: good)
msxmvl has `game_pawn` (character/animation, on par with MSXgl's ~430-line pawn) and a generic
`fsm`. MSXgl additionally ships `game/menu` + `game_menu`, `game/sequence` (scripted
sequences), `game/state`, and `game/bitfield`. These are convenience layers, not hardware, so
the gap is "easier to use," not "can't do." A menu helper and a sequence/cutscene helper are
the two most requested; they are small and align with the ease-of-use value.

### 5. Exotic input devices (severity: minor, effort: S each, fit: neutral)
msxmvl covers keyboard, joystick, and mouse — the three every game needs. MSXgl adds paddle,
light gun, Sega Mega-drive pad (JoyMega), Ninja Tap (multi-tap), JSX, generic MSX-HID, and
printer. Each is a small self-contained port, but the audience per device is tiny. Demand-
driven only.

## The asset-pipeline / tooling gap, in detail

This is the largest *structural* difference and deserves its own verdict. The two libraries
have opposite tooling philosophies:

- **MSXgl** ships a compiled C++ toolchain (Visual Studio solution + FreeImage) that owns the
  whole content path: art → tiles/sprites, waveforms → tables, data → compressed C arrays.
  It is powerful and it is also a heavy, platform-specific dependency to build and audit.
- **msxmvl** ships small, auditable **pure-bash** tools for the code/disk path only
  (`bankpack`, `mkbootdsk`, `mb14blob`). There is no image, sprite, or general music
  converter, and no maths-table generator.

The honest consequence: a beginner following msxmvl's excellent docs can build and boot a ROM,
but the moment they want *their own* graphics or music on screen, the library stops and the
docs go quiet. That is the single place the "easier to use" promise is currently unmet for the
real game/demo use case.

The fix that respects the values is **not** to reproduce MSXtk. It is to add a minimal,
scriptable converter — PNG → SCREEN 2/4 tileset + name table + 16×16 sprite bank — written in a
language that needs no compiled dependency (Python/Pillow, or a tiny bash+netpbm), documented
and drift-guarded like everything else, with a worked "your art on screen in ten minutes"
page. Maths tables (MSXmath's job) are trivial to emit from any host script and need not be a
standalone C++ tool. Compression at author time can piggyback on the existing external
`zx0`/`pletter` encoders once the runtime unpacker (gap #3) exists.

## Where msxmvl is ahead

Kept short and honest — these are real, and they are the reasons to use it:

- **Measured performance, including the losses.** 109/152 core functions benchmarked faster
  than MSXgl by counting T-states in a cycle-accurate emulator, 41 identical, one 2.8% slower
  and published as such. MSXgl makes no comparable per-call cost claims.
- **Docs that cannot rot.** Every code sample is compiled, booted on an emulated MSX, and
  checked; a drift guard fails the build if a doc snippet diverges from its tested example.
- **A *designed* 64 KB escape.** The banking C data model (coordinated per-bank DATA/BSS,
  globals-init via `farrt`, relocatable shadow) and the `FAR_FN`/`--build` ergonomics are
  engineering msxmvl did, not a register wrapper — the measured 169 T far-call is the headline.
- **An interrupt-time-optimised replayer** with a measured worst-case cost — the audio analogue
  of the performance thesis.
- **Pure-bash tooling** with no compiled/FreeImage dependency — trivial to build and audit.
- **Clean-room, permissive licensing** vs MSXgl's CC BY-SA — materially easier for someone
  shipping a commercial game.
- **Flat, readable crt0s** — one flag picks a startup you can actually read.

## Prioritized recommendations

Ordered by (demand × fit-with-values ÷ effort). Build top-down.

1. **ZX0 (or Pletter) runtime decompressor.** Highest leverage: unblocks asset-heavy ROMs,
   is a bounded pure-logic module, has a free oracle (the reference unpacker), and yields
   another measured speed/size claim. Effort **S-M**. Do this first.
2. **One modern tracker replayer (PT3 or Arkos AKM), with a measured interrupt budget.**
   Closes the most visible feature gap for games/demos and extends the "measured replayer"
   story beyond MoonBlaster. Pick the format with the largest living tool ecosystem. **M**.
3. **A minimal, scriptable image→tileset/sprite converter + a "your art on screen" doc.**
   Directly repairs the ease-of-use gap the pipeline creates. Keep it dependency-light and
   drift-guarded; scope to SCREEN 2/4 first. **M-L**. The biggest structural win, so worth the
   weight — but scoped, not a MSXtk clone.
4. **An ayFX-style PSG sound-effects layer.** Cheap, high game demand (SFX layered over music),
   fits the measured-cost method. **S**.
5. **Game menu + sequence helpers.** Round out `game_pawn` for the "make a game" path; small
   and on-theme for ease of use. **S-M**.

### What is *not* worth chasing (unless demand appears)

- **V9990/GFX9000 depth.** Niche hardware, untestable headlessly by design, tiny audience.
  Keep the thin subset; do not invest.
- **Exotic mappers (NEO-8/16, Yamanooto, ASCII16-X, Popolon-SCC) and tape/cassette + BASIC-
  binary targets.** Mainstream targets are already covered; these serve very small audiences.
- **Nextor target, UNAPI/networking, QR, SJIS/Kanji, PCM, exotic input devices.** All genuine
  MSXgl features, all niche. Add reactively when a user actually needs one, not speculatively.
- **Cloning the full MSXtk C++ suite (MSXbin/MSXhex/MSXmath as standalone tools).** Low value:
  binary→array and maths-table emission are a few lines of host script each and do not justify
  a compiled toolchain that would contradict the pure-tooling ethos.
