# Double Buffering (`display`)

Flicker-free page flipping for **SCREEN 5 / 6** (GRAPHIC4, V9938, 128 KB VRAM). Link
`display.c`, include `display.h`. Self-contained — it drives VDP port `0x99` directly, no
dependency on the `gen/` VDP module.

## The model

In SCREEN 5, VRAM holds **four 32 KB pages** of 256 lines each. The VDP scans out exactly one
page (selected by register R#2). The command engine addresses a page by its **Y range**: page
*p* is command-Y `p*256 .. p*256+255`. Double buffering means: show one page while you draw the
next frame into the hidden one, then flip. No erase-while-visible, so no flicker.

`display` keeps two variables — the **front** page (on screen) and the **back** page (hidden,
your draw target) — and swaps them on `Display_Flip()`.

---

## `Display_InitDoubleBuffer` / `GetFrontPage` / `GetBackPage` / `GetBackPageY`

```c
void Display_InitDoubleBuffer(void);   // show page 0, draw into page 1
u8   Display_GetFrontPage(void);       // page currently on screen
u8   Display_GetBackPage(void);        // hidden page — draw here this frame
u16  Display_GetBackPageY(void);       // back page * 256: add to a command's DY
```

`GetBackPageY()` is the one you use every frame: add it to a VDP command's destination Y so the
draw lands in the hidden page rather than the visible one.

**Example — inspect the initial state.**

```c
#include "display.h"
volatile u8 __at(0xE000) R[4];

void main(void)
{
	Display_InitDoubleBuffer();

	R[1] = Display_GetFrontPage();          // 0 (shown)
	R[2] = Display_GetBackPage();           // 1 (hidden, draw here)
	R[3] = (u8)(Display_GetBackPageY() >> 8);   // 256>>8 = 1

	R[0] = (Display_GetFrontPage() == 0 &&
	        Display_GetBackPage()  == 1 &&
	        Display_GetBackPageY() == 256) ? 0xA5 : 0x00;
	for (;;) {}
}
```

Runs to `R[] = a5 00 01 01` — front 0, back 1, back-page Y offset 256. *(tested: `display_01_pages.c`)*

---

## `Display_Flip`

```c
void Display_Flip(void);   // reveal the back page; the old front becomes the new back
```

Swaps front and back: what you just drew becomes visible, and the previously visible page
becomes your next draw target. **It does not wait for vblank** — call
[`VDP_WaitVBlank()`](VBlank-Sync.md) first for a tear-free flip.

**Example — two flips return to the start.**

```c
#include "display.h"
volatile u8 __at(0xE000) R[4];

void main(void)
{
	Display_InitDoubleBuffer();    // front=0 back=1
	Display_Flip();                // front=1 back=0
	R[1] = Display_GetFrontPage(); // 1
	R[2] = Display_GetBackPage();  // 0

	Display_Flip();                // front=0 back=1 again
	R[3] = Display_GetFrontPage(); // 0

	R[0] = (R[1] == 1 && R[2] == 0 && R[3] == 0) ? 0xA5 : 0x00;
	for (;;) {}
}
```

Runs to `R[] = a5 01 00 00`. *(tested: `display_02_flip.c`)*

### The canonical frame loop

```c
Display_InitDoubleBuffer();
for (;;) {
	u16 by = Display_GetBackPageY();   // Y offset of the hidden page
	// ... draw this frame with every command's DY += by ...
	VDP_WaitVBlank();                  // sync to the safe window (vsync module)
	Display_Flip();                    // reveal it
}
```

---

## `Display_ShowPage`

```c
void Display_ShowPage(u8 page);   // low-level: make the VDP scan out `page` (0..3)
```

Writes R#2 = `(page << 5) | 0x1F`. `Init` and `Flip` call this internally; use it directly only
for manual page control (e.g. a static split-screen). It does **not** touch the front/back
bookkeeping.

**Example — select page 3; R#2 must become `0x7F`.**

```c
#include "display.h"
volatile u8 __at(0xE000) R[2];

void main(void)
{
	Display_ShowPage(3);           // R#2 = (3<<5)|0x1F = 0x7F
	R[0] = 0xA5;                   // reached here without hanging; R#2 checked by harness
	for (;;) {}
}
```

The harness reads VDP R#2 back from hardware: `0x7F`, confirming the register math.
*(tested: `display_03_showpage.c`, with `EX_VREG_ASSERT="2=7f"`)*

## See also

- [VBlank Sync](VBlank-Sync.md) — pair `Display_Flip` with `VDP_WaitVBlank` for no tearing.
