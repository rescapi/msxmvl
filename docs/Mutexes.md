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

## Taking and releasing a lock

```c
#include "mutex.h"
volatile u8 __at(0xE000) R[8];

MUTEX_DATA();               // defines g_Mutex — required once per application

#define MTX_VRAM  3         // name your locks

void main(void)
{
	Mutex_Init();                             // all 8 locks free
	R[1] = Mutex_Gate(MTX_VRAM) ? 1 : 0;      // 1 — free

	Mutex_Lock(MTX_VRAM);
	R[2] = g_Mutex;                           // 0x08 — bit 3 set
	R[3] = Mutex_Gate(MTX_VRAM) ? 1 : 0;      // 0 — taken
	R[4] = Mutex_Gate(2) ? 1 : 0;             // 1 — other locks unaffected

	Mutex_Release(MTX_VRAM);
	Mutex_Wait(MTX_VRAM);                     // returns at once: it is free
	R[5] = g_Mutex;                           // 0x00

	R[0] = (R[1] == 1 && R[2] == 0x08 && R[3] == 0 && R[4] == 1 && R[5] == 0)
	     ? 0xA5 : 0x00;
	for (;;) {}
}
```

Runs to `R[] = a5 01 08 00 01 00`. *(tested: `mutex_01_locks.c`)*

## Guarding the VDP against the ISR

The main loop takes the lock around its VRAM work; the ISR *gates* on it and skips its own update if
the lock is held. This is the safe direction — the ISR never blocks.

```c
#define MTX_VRAM  3

void MyIsr(void)                      // called from the interrupt hook
{
	if (!Mutex_Gate(MTX_VRAM))        // main is mid-VRAM-write: leave it alone
		return;
	VDP_Poke_16K(g_HudAddr, g_Frame); // safe this frame
}

void main(void)
{
	Mutex_Init();
	for (;;) {
		Mutex_Lock(MTX_VRAM);
		Draw_FillBox(0, 0, 32, 16, 4);   // ISR will not touch VRAM during this
		Mutex_Release(MTX_VRAM);
		VDP_WaitVBlank();
	}
}
```

**Never call `Mutex_Wait` from an ISR.** It busy-waits, and the only code that can release the lock
is the code you interrupted — that is a guaranteed hang. `Mutex_Wait` is for main-loop code only.

## See also

- [VBlank Sync](VBlank-Sync.md) — the other half of interrupt-safe VDP access.
- [VDP Access](VDP-Access.md) — the resource these locks usually protect.
