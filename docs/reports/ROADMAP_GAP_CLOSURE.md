# Roadmap: closing the MSXgl content gap (full-breadth)

**Status:** PLAN. The action plan derived from [GAP_ANALYSIS_MSXGL.md](GAP_ANALYSIS_MSXGL.md).
Goal: **cover the whole content-plumbing gap** — decompression, music trackers, sound effects,
the asset pipeline, and the game-framework helpers — comprehensively, not one-token-per-area.
The library's thesis is unchanged and applies to every module: clean-room, verified on emulated
hardware, and **smaller AND faster than its reference, both measured** (see the standing
requirement). True-niche hardware breadth is deliberately parked (see the bottom).

## Why breadth here, and why the "reactive default" was wrong

The gap analysis found msxmvl matches MSXgl across the *engine* and lags on *content-plumbing
breadth*. That breadth is **binary for adoption**: if a user's music is WYZ and we don't do WYZ,
our excellent PT3 support is useless to them; if their art is a PNG and we ship no converter,
the docs go quiet. So for the content categories we cover the whole living set, not a sample.

One structural note that makes this both harder and better than MSXgl: MSXgl gets breadth
*cheaply* by **bundling third-party replayers**; msxmvl implements each clean-room and measures
it, so every format costs us more to build but ships as a "smaller and faster than the reference,
verified" claim MSXgl cannot make. The fan-out model below is what makes that sustainable.

## The fan-out principle (first-of-category, then cheap followers)

The expensive part of each module is the **harness**, not the module. So each category is built
**first → fan-out**:

1. The **first** module in a category pays to build the reusable rig: the oracle
   (reference encoder/decoder, or a reference player's register trace), the measurement (size in
   bytes + T-states vs the reference), and the doc/example/CI wiring.
2. Every **subsequent** module in that category reuses that rig — "same harness, different
   decoder / different format parser" — at a fraction of the cost, each adding another measured
   win. Followers are independent and **parallelize** across worktree-isolated agents.

## The playbook (every module)

1. **Spec** — short `docs/*.md`; clean-room (reference read for *facts* only; header provenance
   note). 2. **Implement** — `lib/ext/<mod>` (new capability) or `lib/gen/<mod>` (MSXgl-interface-
   compatible, like `pac`); flat, SDCC `--sdcccall 1`. 3. **Verify** — byte-exact oracle
   (pure-logic) or on-target self-check + measured interrupt budget (runtime); golden output
   (host tools). 4. **Document** — drift-guarded page + example, `Home.md`/`README` rows, CI.
   5. **Land** — PR to `msxmvl`, then `make_public` → `msxmvl`; batch a category into one public
   PR where sensible.

**Standing requirement — smaller AND faster, both verified.** Every runtime module is measured
against its reference (MSXgl's, or the format's canonical Z80 implementation) on **both size
(bytes) and speed (T-states)**, published side by side — a gate, not a nice-to-have. The bar is
calibrated to what the reference actually is:

- **Reference is beatable (higher-level C, or a bundled third-party player never cycle-tuned):**
  we **beat both**, measured. This is the common case and the default expectation.
- **Reference is an already-optimal hand-tuned primitive (on the ROM-safe Pareto floor — e.g.
  the ~70-byte ZX0/ZX7 decoders):** a clean-room implementation converges to its exact budget, so
  "beat both" is physically unavailable. The honest bar is then: **match it exactly (verified
  byte-for-byte), win on integration (a zero-overhead C API, VRAM-streaming, etc. the asm
  reference lacks) and on verification (an oracle test suite it ships none of), AND, where a
  different constraint permits a faster form, ship that too** — e.g. a RAM-only self-modifying
  variant that beats the ROM-safe floor on speed. Establish the tie/win honestly with numbers
  (the ZX0 module does exactly this: `UnZX0` ties the ROM-safe reference, `UnZX0_fast` beats it
  for RAM targets).

A tie/loss is always published, never buried (as the core benchmark already does: "one function
2.8% slower, and here it is"). Host tools are judged on output size/quality and dependency-
lightness instead.

Effort key: **S** ≈ a session, **M** ≈ a few, **L** ≈ a sustained campaign.

---

## Category A — Decompression  (unblocks asset-heavy ROMs; highest leverage; SHIPPED)

RLEp is all msxmvl ships today. Cover the codecs with real MSX usage; each is a bounded,
pure-logic, byte-exact-oracle module — the shape msxmvl does best, and each a measured win.

- **A1 ZX0** — *first / harness-builder* (**S–M**, in progress). Best modern ratio, tiny fast
  Z80 decoder, permissive. Builds the corpus/round-trip oracle + the size/speed-vs-reference rig
  every follower reuses. RAM + VRAM-stream variants.
- **A2 Pletter** — (**S**) the long-standing MSX-scene default; many existing assets are Pletter.
- **A3 ZX7** — (**S**) ZX0's predecessor; still common; trivial once A1's rig exists.
- **A4 LZSA / LZ48** — (**S**) fast-decode family MSXgl ships; for speed-critical unpack.
- **A5 aPLib / Bitbuster** — (**S** each, *reactive*) add if a shipped asset needs them.
Author-time: piggyback the existing external encoders (`zx0`, `pletter`, …); the asset pipeline
(Category D) wraps them. Each publishes an ours-vs-reference size/speed table.

## Category B — Music trackers  (the clearest feature gap; parallel to A)

msxmvl plays only MoonBlaster 1.4. Cover the living tracker set. **B1 builds the harness**
(reference-player register-trace oracle + an `experiments/<fmt>-irqtime` interrupt-budget rig,
the MoonBlaster/mb14 method); the rest fan out. Each ships a **measured worst-case + average
interrupt T-state** headline — a claim MSXgl makes for none of its bundled players.

- **B1 Arkos Tracker 2 (AKM/AKY)** — *first / harness-builder* (**M**). Modern default, actively
  maintained, cross-platform, well-documented format, reference asm players to trace against.
  <https://www.julien-nevo.com/arkostracker/>
- **B2 PT3 (Vortex)** — (**M→S** on B1's rig). The classic; the largest back-catalogue.
  <https://bulba.untergrund.net/vortex_e.htm>
- **B3 WYZ** — (**S–M**) PSG `.MUS`, used by CPC/MSX/Spectrum games; asm replayer published.
  <https://github.com/AugustoRuiz/WYZTracker>
- **B4 TriloTracker** — (**M**) MSX-native **PSG + SCC / PSG + FM** — the SCC music MSXgl barely
  covers; our differentiator. <https://github.com/cornelisser/TriloTracker>
- **B5 VGM / lVGM playback** — (**M**) chip-register-log player (PSG/SCC/OPLL); not a tracker but
  the universal interchange format.
- *(NDP, MGLV: parked — negligible living use.)*
Per-format caveat: bundle only license-cleared example songs (as the MoonBlaster C.v/d Geest
song was); otherwise generate a trivial original.

## Category C — Sound effects  (completes game audio; after B1)

- **C1 ayFX-style PSG SFX layer** — (**S**) `Init/Play/Update`, channel-priority mixing with an
  explicit documented contract for which PSG channel SFX may steal from the music driver;
  interaction test proving the music is undisturbed and restored. Measured per-interrupt cost.

## Category D — Asset pipeline  (the biggest *structural* gap; repairs "easier to use")

The one place the docs currently go quiet the moment a dev brings their own art. **Not** a clone
of MSXgl's compiled C++ MSXtk — a minimal, scriptable, dependency-light path (Python+Pillow, or
bash+netpbm), deterministic, golden-tested, drift-guarded. **D1 builds the converter core**; the
rest extend it.

- **D1 image → SCREEN 2/4 tileset + de-duped name table + 8×8/16×16 sprite bank** — *first*
  (**M–L**). Emits C/asm arrays (folds in the trivial bin→array job), optionally ZX0-packed
  (consumes A1). A "your art on screen in ten minutes" drift-guarded doc.
- **D2 SCREEN 5/7/8 (bitmap/MSX2) image import** — (**M**) once D1's pipeline exists.
- **D3 maths-table generator** (sin/cos/atan fixed-point — MSXmath's job) — (**S**) a few lines of
  host script, folded into the tool; no standalone C++.
- **D4 tilemap / palette helpers** — (**S–M**) authoring conveniences on D1.

## Category E — Game framework  (ease-of-use; independent)

`game_pawn` + `fsm` exist. Add the convenience layers MSXgl ships, built on `input`/`print`/
`vsync`, verified by scripted-input self-check drivers (`test/ext` style):

- **E1 game_menu** (**S–M**) — data-driven menu (items, cursor, callbacks).
- **E2 game_seq** (**S–M**) — scripted sequence / cutscene runner on `fsm`.
- **E3 game_state / bitfield** (**S**) — small state-stack + packed-flag helpers.

---

## Parked — reactive only (build when a real user needs one)

Genuine MSXgl features, deliberately **not** scheduled: matching them is completionism with tiny
audiences and dilutes the smaller+faster-verified focus that is our edge.

- **Exotic input** — paddle, light gun, JoyMega, Ninja Tap, JSX, generic HID, printer. (Keyboard/
  joystick/mouse already shipped.)
- **Exotic ROM mappers** — NEO-8/16, Yamanooto, ASCII16-X, Popolon-SCC. The bankpack mapper table
  extends cleanly (`Code-Banking-Mappers.md`) when a publisher needs one.
- **Tape/cassette + MSX-BASIC-binary targets; Nextor storage; UNAPI/networking (ObsoNET);
  QR / SJIS-Kanji; PCM (pcmenc); V9990/GFX9000 depth.** All niche; keep the thin V9990 subset;
  add reactively, never speculatively.

## Sequencing, parallelism, milestones

```
Cat A  decompression   A1 ─(rig)→ A2 A3 A4 (parallel followers)
Cat B  trackers        B1 ─(rig)→ B2 B3 B4 B5 (parallel followers)   ┐
Cat C  sound effects            C1 (after B1; shares PSG)            ├─ Milestone: "ship a game"
Cat D  asset pipeline  D1 ─→ D2 D3 D4 (D1 optionally consumes A1)    │   art + music + sfx + ui
Cat E  game framework  E1 E2 E3 (independent, any time)              ┘
```

- **A and B run in parallel now** (independent; different files). Each category's *first* module
  is on the critical path; its *followers* fan out to parallel agents once the rig lands.
- **C** follows **B1** (needs the PSG-sharing interaction test). **D** optionally consumes **A1**
  (ZX0-packed assets). **E** is independent.
- To avoid the shared-index-file conflicts two doc-adding modules cause (`run_all.sh`,
  `verify_docs.sh`, `Home.md`), land the *first* of each category solo, then batch its followers.
- **Milestone "ship a game":** at least A1–A2, B1–B2, C1, D1, E1 landed — a developer can compress
  assets, convert their own art, play their own music + SFX, and drive menus/cutscenes, entirely
  within msxmvl. That closes the content gap the analysis identified.

## Success criteria

The content-plumbing categories (A–E) covered across their living format sets — each module
clean-room, oracle/emulator-verified, and published smaller+faster than its reference. The
niche-hardware breadth consciously parked with written reasons. Breadth achieved without ever
diluting the depth (measured, verified, readable) that distinguishes the library.
