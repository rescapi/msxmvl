# Memory Operations (`memory`)

Block copy and fill for main RAM — the Z80 equivalents of `memcpy`/`memset`, plus faster
unrolled variants for hot paths. Link `memory.c`, include `memory.h`.

---

## `Mem_Set` / `Mem_Copy`

```c
void Mem_Set(u8 val, void* dest, u16 size);
void Mem_Copy(const void* src, void* dest, u16 size);
```

Fill `size` bytes with `val`; copy `size` bytes from `src` to `dest`. Note the argument order:
`Mem_Set` takes the **value first**, and both take `dest` before `size`.

```c
#include "memory.h"
volatile u8 __at(0xE000) R[8];

void main(void)
{
	u8 a[4], b[4];

	Mem_Set(0x77, a, 4);           // a = { 77, 77, 77, 77 }
	Mem_Copy(a, b, 4);             // b <- a

	R[1] = a[0]; R[2] = a[3];
	R[3] = b[0]; R[4] = b[3];
	R[0] = (a[0]==0x77 && a[3]==0x77 && b[0]==0x77 && b[3]==0x77) ? 0xA5 : 0x00;
	for (;;) {}
}
```

Runs to `R[] = a5 77 77 77 77`. *(tested: `memory_01_copyset.c`)*

---

## `Mem_FastSet` / `Mem_FastCopy`

```c
void Mem_FastSet(u8 val, void* dest, u16 size);
void Mem_FastCopy(const void* src, void* dest, u16 size);
```

Same contract, unrolled-`LDI` implementations that are faster for larger blocks (verified
byte-identical to the plain versions, no small-size penalty). Reach for these on hot copy/clear
paths — e.g. blitting a buffer or clearing a work area every frame.

```c
#include "memory.h"
volatile u8 __at(0xE000) R[8];

void main(void)
{
	u8 a[8], b[8];

	Mem_FastSet(0x5A, a, 8);       // a = eight 0x5A bytes
	Mem_FastCopy(a, b, 8);         // b <- a

	R[1] = b[0]; R[2] = b[7];
	R[0] = (b[0]==0x5A && b[7]==0x5A) ? 0xA5 : 0x00;
	for (;;) {}
}
```

Runs to `R[] = a5 5a 5a`. *(tested: `memory_02_fast.c`)*

> For copying to/from **VRAM**, use the VDP block functions instead
> ([`VDP_WriteVRAM_16K` / `VDP_ReadVRAM_16K`](VDP-Access.md)) — VRAM isn't CPU-addressable.
> For copying to/from **mapper RAM beyond 64 KB**, use [`Far_Read` / `Far_Write`](Far-Pointers.md).

## See also

- [VDP Access](VDP-Access.md) — the VRAM counterpart of these operations.
