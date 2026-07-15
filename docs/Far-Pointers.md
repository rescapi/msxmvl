# Far Pointers (`farmem`)

The ergonomic layer over [segment windowing](Segment-Windowing.md): address any byte in mapper
RAM by a **handle**, and copy blocks in/out with automatic boundary handling. You never touch a
raw window address or a segment register. Link `memseg.c farmem.c`, include `farmem.h`.

## `FarPtr` and `FAR`

```c
typedef u32 FarPtr;                 // a linear address into mapper space
#define FAR(seg, ofs)  ...          // build one: segment + in-segment offset (0..0x3FFF)
#define FAR_ADD(p, n)  ...          // advance a FarPtr, carrying across segments
```

A `FarPtr` is a single 32-bit value (`seg*16KB + ofs`), so SDCC passes it in registers and
pointer arithmetic carries across segment boundaries for free. Call `MemSeg_Init(home)` once
before using any `Far_*` function.

---

## `Far_Peek` / `Far_Poke`

```c
u8   Far_Peek(FarPtr p);
void Far_Poke(FarPtr p, u8 val);
```

Single-byte read/write by handle.

```c
#include "farmem.h"
volatile u8 __at(0xE000) R[4];

void main(void)
{
	MemSeg_Init(16);

	Far_Poke(FAR(17, 0x0100), 0x7E);        // write one byte in segment 17
	Far_Poke(FAR(18, 0x0100), 0x3C);        // ...and one in segment 18

	R[1] = Far_Peek(FAR(17, 0x0100));       // 0x7E — segments are independent
	R[2] = Far_Peek(FAR(18, 0x0100));       // 0x3C
	R[0] = (R[1] == 0x7E && R[2] == 0x3C) ? 0xA5 : 0x00;
	for (;;) {}
}
```

Runs to `R[] = a5 7e 3c`. *(tested: `farmem_01_pokepeek.c`)*

> Per-byte access maps the segment on every call. For more than a few bytes, use the block
> functions below — they map once and `LDIR` the whole run.

---

## `Far_Read` / `Far_Write`

```c
void Far_Read (FarPtr src, void* dst, u16 n);   // mapper RAM -> local buffer
void Far_Write(FarPtr dst, const void* src, u16 n);   // local buffer -> mapper RAM
```

Block copy in either direction — map once, copy the run. This is how you stash a scratch buffer
or an asset into high RAM and fetch it back.

```c
#include "farmem.h"
volatile u8 __at(0xE000) R[8];

void main(void)
{
	u8 src[4] = { 0xDE, 0xAD, 0xBE, 0xEF };
	u8 back[4];
	MemSeg_Init(16);

	Far_Write(FAR(19, 0x0000), src, 4);     // stash 4 bytes into segment 19
	Far_Read(FAR(19, 0x0000), back, 4);     // read them back

	R[1] = back[0]; R[2] = back[1]; R[3] = back[2]; R[4] = back[3];
	R[0] = (back[0]==0xDE && back[1]==0xAD && back[2]==0xBE && back[3]==0xEF)
	       ? 0xA5 : 0x00;
	for (;;) {}
}
```

Runs to `R[] = a5 de ad be ef`. *(tested: `farmem_02_readwrite.c`)*

### Crossing a segment boundary

An object may straddle the 16 KB edge between two segments. `Far_Read`/`Far_Write` **auto-split**
at the boundary — you don't special-case it.

```c
#include "farmem.h"
volatile u8 __at(0xE000) R[8];

void main(void)
{
	u8 src[4] = { 0xC0, 0xC1, 0xC2, 0xC3 };
	u8 back[4];
	MemSeg_Init(16);

	// start two bytes before the end of segment 19 -> spills into segment 20
	Far_Write(FAR(19, 0x3FFE), src, 4);
	Far_Read (FAR(19, 0x3FFE), back, 4);

	R[1] = Far_Peek(FAR(19, 0x3FFF));       // 0xC1 — last byte of seg 19
	R[2] = Far_Peek(FAR(20, 0x0000));       // 0xC2 — first byte of seg 20
	R[0] = (back[0]==0xC0 && back[1]==0xC1 && back[2]==0xC2 && back[3]==0xC3 &&
	        R[1]==0xC1 && R[2]==0xC2) ? 0xA5 : 0x00;
	for (;;) {}
}
```

Runs to `R[] = a5 c1 c2` — the first two bytes landed in segment 19, the last two in segment 20.
*(tested: `farmem_03_boundary.c`)*

---

## `Far_Set`

```c
void Far_Set(FarPtr dst, u8 val, u16 n);   // fill n bytes with val (boundary auto-split)
```

Clear or initialize a region — often the first thing you do to a fresh multi-segment scratch
buffer.

```c
#include "farmem.h"
volatile u8 __at(0xE000) R[4];

void main(void)
{
	MemSeg_Init(16);

	Far_Set(FAR(22, 0x3FFE), 0xEE, 4);      // 4 bytes spanning segment 22 -> 23

	R[1] = Far_Peek(FAR(22, 0x3FFF));       // 0xEE (in seg 22)
	R[2] = Far_Peek(FAR(23, 0x0000));       // 0xEE (in seg 23)
	R[0] = (R[1] == 0xEE && R[2] == 0xEE) ? 0xA5 : 0x00;
	for (;;) {}
}
```

Runs to `R[] = a5 ee ee`. *(tested: `farmem_05_set.c`)*

---

## `Far_With` — the scoped borrow

```c
typedef void (*FarBorrowFn)(const void* win, void* ctx);
void Far_With(FarPtr p, u16 len, FarBorrowFn fn, void* ctx);
```

Maps `p`'s segment, hands your callback a **real pointer** to the data (valid only for the
duration of the call), then restores the previous segment. Because the pointer can't escape the
callback, the classic "kept a pointer into a segment that got paged out" bug is structurally
impossible. Use it to read or edit a far struct **in place** without copying it to a local.

```c
#include "farmem.h"
volatile u8 __at(0xE000) R[4];

static u8 g_seen;
static void inspect(const void* win, void* ctx)
{
	(void)ctx;
	g_seen = *(const volatile u8*)win;      // read the mapped byte in-scope
}

void main(void)
{
	MemSeg_Init(16);
	Far_Poke(FAR(21, 0x0000), 0x99);

	Far_With(FAR(21, 0x0000), 1, inspect, (void*)0);

	R[1] = g_seen;                          // 0x99
	R[2] = MemSeg_GetWindow();              // 16 — Far_With restored home
	R[0] = (g_seen == 0x99 && R[2] == 16) ? 0xA5 : 0x00;
	for (;;) {}
}
```

Runs to `R[] = a5 99 10` — the callback saw the mapped `0x99`, and afterwards the window is back
to the home segment (`0x10 = 16`). *(tested: `farmem_04_with.c`)*

> **Golden rule:** never return or store a pointer into the window from inside `Far_With` (or
> after any `MemSeg_Window`). Copy the bytes out (`Far_Read`) or do the work inside the callback.
> `Far_With` exists to make that the easy path.

## See also

- [Segment Windowing](Segment-Windowing.md) — the `memseg` primitives underneath.
- [Code Banking](Code-Banking.md) — the sibling problem: running more *code* than fits in 64 KB.
