# Interrupts (IM 2 takeover)

Every MSX interrupt normally runs the **whole BIOS interrupt handler** — register shuffling,
status reads, the keyboard scan, the hook chain — before your code (an H.TIMI hook) sees
anything. The `isr` module bypasses all of it: **IM 2** vectors through a RAM table straight
to a dispatcher that saves registers, acknowledges the VDP, and calls your plain C function.

Measured on the same emulated hardware as everything else: from the interrupt vector, **your
handler is running in 164 T-states**; the BIOS path takes **251 T-states just to reach an
H.TIMI hook** — and then keeps running its full ISR after your hook returns, while the `isr`
dispatcher is done in ~60. It works on any program type — 16/32/48 KB ROMs and banked
MegaROMs — because IM 2 never needs page 0.

```c
// im2.c — take over the interrupt: your handler IS the 50 Hz tick.
#include "types.h"
#include "isr.h"

volatile u16 g_Ticks;

// An ordinary C function — the module saves/restores registers around it.
void vblank_handler(void)
{
	if (ISR_Status() & 0x80)     // bit 7: this interrupt is a VBLANK
		++g_Ticks;
}

void start_counting(void)
{
	g_Ticks = 0;
	ISR_InstallIM2(vblank_handler);
}
```

After `ISR_InstallIM2`, `g_Ticks` advances all by itself; `ISR_UninstallIM2()` returns to
IM 1 and the counting stops on the spot. *(tested: `isr_01_im2.c` — both directions asserted)*

## The contract

- **While installed, the BIOS interrupt work does not happen.** No JIFFY tick, no BIOS
  keyboard scan, no H.TIMI hooks — your handler is the interrupt. (The `input` module still
  works: it reads the keyboard matrix directly.)
- The handler is an ordinary `void(void)` C function; the dispatcher saves and restores the
  main register set plus IX/IY. The alternate set is not touched — SDCC-generated code never
  uses it. Keep the handler short: it runs inside the interrupt.
- `ISR_Status()` returns the VDP S#0 byte captured (and thereby acknowledged) at entry —
  bit 7 distinguishes VBLANK from other VDP interrupts.
- RAM use is fixed and documented: the 257-byte vector table at `0xE300` and a 3-byte thunk
  at `0xE4E4` (both overridable with `-D` if your program owns that RAM).

## Which interrupt path when?

| Path | Cost to your code | BIOS still alive? | Use when |
|---|---|---|---|
| H.TIMI hook (`bios_hook`) | ~251 T + BIOS ISR around it | yes | you want JIFFY, BIOS key scan, other hooks |
| `isr` IM 2 takeover | ~164 T, nothing else runs | no | per-frame game code, maximum time in your hands |
| `crt0_rom48` RAM vector | direct IM 1, BIOS already gone | no (by format) | 48 KB carts — see [ROM Formats](ROM-Formats.md) |
