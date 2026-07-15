# msxmvl reference/

Authoritative hardware docs, saved locally for reuse across sessions.

| File | What | Use |
|------|------|-----|
| `V9938_Technical_Data_Book_Aug85.pdf` | Yamaha V9938 MSX-VIDEO Technical Data Book, Aug 1985 (170 pp, primary source) | full register/command/electrical detail |
| `V9938_REFERENCE.md` | Distilled, easy-readable version | day-to-day: VRAM timing, ports, registers, status, command engine — the facts the harness, static min-gap checker, and VDP primitives need |

**Start with `V9938_REFERENCE.md`**; drop to the PDF only for detail it doesn't cover.

Key number to remember: **VRAM min-gap = 15 T (screens 1–8), 20 T (screen 0); no gap when
blanked.** `OUTI`=18 T/byte is safe everywhere; `OUT (n),A`=12 T is blanked-only.

Re-download the PDF if missing:
`curl -sL -o V9938_Technical_Data_Book_Aug85.pdf https://www.bitsavers.org/pdf/yamaha/Yamaha_V9938_MSX-Video_Technical_Data_Book_Aug85.pdf`
