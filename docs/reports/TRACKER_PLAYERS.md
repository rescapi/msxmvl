# Music tracker players — campaign result (Category B + C)

End-to-end characterisation of the MSX tracker-player landscape for msxmvl: five
reference baselines you can audition, per-player optimisation verdicts with measured
size + interrupt T-states (verified byte-identical via a chip-state oracle), our own
VGM player, and the reusable measurement/oracle rigs. All work lives under
`experiments/*` (reproducible `build.sh` / `measure.sh` each; third-party player
sources are MIT-attributed or used with the owner's clearance, git-ignored where the
licence is informal).

## Reference baselines (audition ROMs/disks)

Boot each in openMSX (`-machine Philips_NMS_8245`) to hear the reference player:

| Format | Artifact | Player / licence | Runs on |
|---|---|---|---|
| Arkos Tracker 2 (AKG) | `ARKOS.dsk` | Julien Névo (MIT) | disk |
| PT3 / Vortex | `PT3.rom` | S.V. Bulba, mvac7 SDCC (scene-std) | cart |
| WYZTracker | `WYZ.rom` | Augusto Ruiz (MIT) | cart |
| TriloTracker (PSG+SCC) | `TRILO.rom` | cornelisser (cleared) | 128 KB Konami-SCC cart |
| VGM (AY log) | `VGM.rom` | **ours** (clean-room) | cart |

## Optimisation verdicts (measured on "typical song", Z80 @ 3.58 MHz, openMSX)

| Player | Code size | avg / max T per frame | Verdict |
|---|---:|---:|---|
| AKG (spke & uniabis) | 2953 B | 2972 / 5314 | **match** — expert-tuned |
| PT3 (SapphiRe/Bulba) | 1680 B | 5632 / 5727 | **match** — 99% channels active, no idle slack |
| WYZ (Ruiz) | — | 4718 / 10646 | **match** — variance is structural decode, not slack |
| Trilo (cornelisser) | — | 9164 / 17537 | **match** — SCC waveform reload already gated on change |
| ayFX SFX (mvac7) | — | ~1157 (mix) | **match** — tuned scene-standard SFX player |

**Conclusion (evidence-based, profiled *and* code-read per player):** none of these
third-party players carry redundant-work slack. The mb14/MBFM optimisation wins
existed because *our own* players wastefully scanned idle channels; these reference
players don't. As with the ZX0/LZSA2 decoders, the honest bar for an already-tuned
reference is **match + integrate**, not beat: T-state cost matched and proven
byte-identical via each player's PSG(/SCC) oracle, wrapped in a zero-overhead C API,
an on-target oracle test suite the references ship none of, and cleared/attributed
provenance — published side-by-side. The one from-scratch module, the **VGM player**,
is ours outright (AY register-log replay with MSX-clock rescaling).

## Reusable assets

- `experiments/{arkos,pt3,wyz,trilo,vgm}-jukebox/` — the audition builds.
- `experiments/{akg,wyz,pt3,trilo}-player/` — measurement rigs (`measure.sh`) +
  chip-state oracles (`*_trace.txt`) + modifiable working copies.
- `experiments/akg-player/capture_oracle.sh` — per-frame PSG-trace oracle capture.
- `experiments/trilo-player/build_from_source.sh` — a Linux **Sjasm** build + a
  Trilo re-player source build that plays identically to the prebuilt ROM.
- Per-format toolchains proven: rasm (Arkos), pasmo (WYZ, tniASM→pasmo port), SDCC
  (PT3/ayFX), Sjasm (Trilo).
