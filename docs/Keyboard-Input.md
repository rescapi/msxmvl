# Keyboard Input (`input`)

Read the keyboard from the MSX key matrix. Link `input.c`, include `input.h`. The module also
covers joysticks (`Joystick_Read`/`Joystick_Update`) and mouse (`Mouse_Read`); this page focuses
on the keyboard, which is the most common and shows the two idioms you'll reuse everywhere.

## The key matrix

MSX keys live in an 8-column × 11-row matrix read through the PPI. A key is named
`MAKE_KEY(row, bit)` and the header defines constants for all of them (`KEY_SPACE`, `KEY_UP`,
`KEY_A`, …). Reads are **active-low**: a `0` bit means *pressed*.

There are two ways to read, depending on whether you want "is it down now" or "was it just
pressed".

---

## `Keyboard_Read` — is a key down right now

```c
u8 Keyboard_Read(u8 row);   // 8 column bits of `row`; a 0 bit = key held (active-low)
```

The direct, low-level read of one matrix row. Test a specific key by masking its bit. `KEY_SPACE`
is row 8, bit 0:

```c
#include "input.h"
volatile u8 __at(0xE000) R[2];

void main(void)
{
	for (;;) {
		u8 row8 = Keyboard_Read(8);         // bit 0 = SPACE (0 = pressed)
		if ((row8 & 1) == 0)
			R[0] = 0xA5;                    // SPACE is currently down
	}
}
```

Runs to `R[] = a5` while SPACE is held. Good for movement keys and anything where "held" is what
you want. *(tested: `input_01_keydown.c`, harness holds row 8 bit 0)*

---

## `Keyboard_Update` + `Keyboard_IsKeyPushed` — was a key just pressed

```c
void Keyboard_Update(void);              // rescan the matrix: old <- new, new <- matrix
bool Keyboard_IsKeyPushed(u8 key);       // true only on the up->down edge of `key`
void Keyboard_SetBuffer(u8* new, u8* old);   // point the scan at your own RAM
```

For menus and one-shot actions you want the **edge** — true only on the frame a key goes down, not
every frame it's held. `Keyboard_Update` scans into a two-generation buffer (current + previous)
and `Keyboard_IsKeyPushed` compares them.

> **Own the buffer.** By default the scan uses the BIOS keyboard work area, which the interrupt
> handler also writes — causing phantom presses. Call `Keyboard_SetBuffer` once with your own
> `new`/`old` arrays (11 bytes each) so your program owns the key state.

```c
#include "input.h"
volatile u8 __at(0xE000) R[2];

static u8 kb_new[11], kb_old[11];      // this program's key state (current + previous)

void main(void)
{
	Keyboard_SetBuffer(kb_new, kb_old);

	for (;;) {
		Keyboard_Update();                 // rescan: old <- new, new <- matrix
		if (Keyboard_IsKeyPushed(KEY_SPACE))
			R[0] = 0xA5;                    // fired on the SPACE press edge
	}
}
```

Runs to `R[] = a5` — the edge fired once when SPACE went down. Call `Keyboard_Update` exactly
once per frame (e.g. right after `VDP_WaitVBlank`). *(tested: `input_02_pushed.c`)*

## Beyond the keyboard

`input.h` also provides `Joystick_Read` / `Joystick_Update` (with the same held-vs-pressed pair),
`Input_Detect` to probe what's plugged into a port, and `Mouse_Read`. They follow the same
conventions shown here.

## See also

- [VBlank Sync](VBlank-Sync.md) — call `Keyboard_Update` once per field, paced by `VDP_WaitVBlank`.
