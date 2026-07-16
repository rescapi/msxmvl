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

**Example — bring up double buffering and name the pages your renderer draws to.**

```c
// render.c — set up double buffering and find the hidden page to draw into.
#include "display.h"

// Bring up double buffering: show page 0, draw into page 1.
void render_init(void)
{
	Display_InitDoubleBuffer();
}

u8  visible_page(void) { return Display_GetFrontPage(); }   // page on screen
u8  draw_page(void)    { return Display_GetBackPage(); }    // hidden page, draw here
u16 draw_page_y(void)  { return Display_GetBackPageY(); }   // Y offset of the hidden page
```

After `render_init()`, `visible_page()` is 0 (shown), `draw_page()` is 1 (hidden), and
`draw_page_y()` is 256 — the Y offset you add to every draw so it lands off-screen.
*(tested: `display_01_pages.c`)*

---

## `Display_Flip`

```c
void Display_Flip(void);   // reveal the back page; the old front becomes the new back
```

Swaps front and back: what you just drew becomes visible, and the previously visible page
becomes your next draw target. **It does not wait for vblank** — call
[`VDP_WaitVBlank()`](VBlank-Sync.md) first for a tear-free flip.

**Example — present a finished frame; two flips return to the start.**

```c
// render.c — flip the freshly drawn page onto the screen.
#include "display.h"

// Reveal the frame we just drew; the old screen becomes the next draw target.
void present_frame(void)
{
	Display_Flip();
}

u8 visible_page(void) { return Display_GetFrontPage(); }   // page on screen
u8 draw_page(void)    { return Display_GetBackPage(); }    // hidden page, draw here
```

Starting from front 0 / back 1, one `present_frame()` makes `visible_page()` read 1 and
`draw_page()` read 0; a second `present_frame()` returns to front 0. *(tested: `display_02_flip.c`)*

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

**Example — park the display on a static title page; R#2 must become `0x7F`.**

```c
// render.c — manually select which page the VDP scans out.
#include "display.h"

#define PAGE_TITLE  3      // the page holding a full-screen title image

// Park the display on the title page.
void show_title_screen(void)
{
	Display_ShowPage(PAGE_TITLE);
}
```

`show_title_screen()` selects page 3, so the harness reads VDP R#2 back as `0x7F` —
`(3 << 5) | 0x1F`, confirming the register math. *(tested: `display_03_showpage.c`, with
`EX_VREG_ASSERT="2=7f"`)*

## See also

- [VBlank Sync](VBlank-Sync.md) — pair `Display_Flip` with `VDP_WaitVBlank` for no tearing.
