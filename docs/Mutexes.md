# Mutexes (`mutex`)

Eight independent binary locks packed into a single byte. Use them to stop an interrupt handler and
your main loop from touching the same resource at the same time — typically the VDP. Include
`mutex.h`; the module is **header-only inline**, so there is no `.c` to link.

## Why you need this

An MSX ISR fires ~60 times a second, in the middle of whatever your main loop was doing. If the ISR
touches the VDP while `main` is halfway through a VRAM address setup, both get corrupted. A lock
byte lets the ISR notice and skip a frame instead.

These are **cooperative** locks — plain read/modify/write on one byte, not atomic test-and-set. They
work because there is only one CPU and one preemptor (the ISR): the ISR *checks* what `main` *sets*.
They will not save you from two writers racing.

## API

```c
MUTEX_DATA();                  // declare the storage byte once, at file scope

void Mutex_Init(void);         // release all 8
void Mutex_Lock(u8 mutex);     // take lock 0..7
void Mutex_Release(u8 mutex);  // release it
bool Mutex_Gate(u8 mutex);     // TRUE if free
void Mutex_Wait(u8 mutex);     // busy-wait until free
```

`MUTEX_DATA()` defines `g_Mutex`. The module deliberately does not define it for you, so it costs
nothing if you never use it.

## Guarding the sprite table

Give the shared resource one named lock and wrap it in verbs. Here the resource is the sprite
table: the main loop claims it while it rewrites it, and the interrupt handler will ask
`sprites_free()` before drawing. Taking the lock sets bit 3 of `g_Mutex` (`0x08`); the other seven
locks are untouched.

```c
// sprite_lock.c — one lock that keeps the interrupt handler off the sprite table
// while the main loop is halfway through rewriting it.
#include "mutex.h"

MUTEX_DATA();               // reserves g_Mutex — once per application

#define LOCK_SPRITES  3     // name the lock instead of hard-coding bit 3

void sprites_init(void)   { Mutex_Init(); }
void sprites_lock(void)   { Mutex_Lock(LOCK_SPRITES); }
void sprites_unlock(void) { Mutex_Release(LOCK_SPRITES); }
bool sprites_free(void)   { return Mutex_Gate(LOCK_SPRITES); }
```

After `sprites_init()` every lock is free, so `sprites_free()` is true. `sprites_lock()` makes
`g_Mutex == 0x08` and `sprites_free()` false, while a different lock still reads free.
`sprites_unlock()` clears it back to `0x00`. *(tested: `mutex_01_locks.c`)*

## Using it across the main loop and the ISR

The main loop takes the lock around its sprite-table work; the ISR *gates* on it with
`sprites_free()` and skips its own update if the lock is held. This is the safe direction — the ISR
never blocks.

```c
void MyIsr(void)                      // called from the interrupt hook
{
	if (!sprites_free())              // main is mid-update: leave the table alone
		return;
	Sprite_Update();                  // safe this frame
}

void main(void)
{
	sprites_init();
	for (;;) {
		sprites_lock();
		Sprite_SetPosition(0, x, y);  // ISR will not read the table during this
		sprites_unlock();
		VDP_WaitVBlank();
	}
}
```

**Never call `Mutex_Wait` from an ISR.** It busy-waits, and the only code that can release the lock
is the code you interrupted — that is a guaranteed hang. `Mutex_Wait` is for main-loop code only.

## See also

- [VBlank Sync](VBlank-Sync.md) — the other half of interrupt-safe VDP access.
- [VDP Access](VDP-Access.md) — the resource these locks usually protect.
