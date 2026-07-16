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

The direct, low-level read of one matrix row. Test a specific key by masking its bit. Here
`KEY_SPACE` (row 8, bit 0) is the jump button — reading "is it held" every frame is exactly what
you want for jump, thrust, or movement:

```c
// controls.c — is the jump button held down right now?
#include "input.h"

#define JUMP_ROW  8      // SPACE lives in matrix row 8...
#define JUMP_BIT  0x01   // ...as bit 0 (active-low: 0 = pressed)

// True while the player is holding the jump button.
u8 jump_held(void)
{
	return (Keyboard_Read(JUMP_ROW) & JUMP_BIT) == 0;
}
```

`jump_held()` masks SPACE's bit and, because reads are active-low, returns true when it is `0`.
Good for movement keys and anything where "held" is what you want. *(tested:
`input_01_keydown.c`, harness holds row 8 bit 0)*

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
// controls.c — did the player just press fire this frame (a fresh press, not a hold)?
#include "input.h"

static u8 kb_new[11], kb_old[11];      // this game's key state (current + previous)

// Call once at startup so the game owns its key-state buffers.
void controls_init(void)
{
	Keyboard_SetBuffer(kb_new, kb_old);
}

// Call once per frame; true only on the frame FIRE (SPACE) was pressed.
u8 fire_pressed(void)
{
	Keyboard_Update();                 // rescan: old <- new, new <- matrix
	return Keyboard_IsKeyPushed(KEY_SPACE);
}
```

`fire_pressed()` returns true once per press, not once per frame held — one shot for firing or
confirming a menu. Call it (and `Keyboard_Update`) exactly once per frame, e.g. right after
`VDP_WaitVBlank`. *(tested: `input_02_pushed.c`)*

## Beyond the keyboard

`input.h` also provides `Joystick_Read` / `Joystick_Update` (with the same held-vs-pressed pair),
`Input_Detect` to probe what's plugged into a port, and `Mouse_Read`. They follow the same
conventions shown here.

## See also

- [VBlank Sync](VBlank-Sync.md) — call `Keyboard_Update` once per field, paced by `VDP_WaitVBlank`.
