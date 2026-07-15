// msxmvl extension module: SCREEN 5/6 double-buffer page management (V9938, 128K VRAM).
// NOT part of the MSXgl API — this is an msxmvl-original helper. Self-contained: it drives
// the VDP directly (no dependency on the gen/ VDP module), so it can be linked on its own.
//
// Model (GRAPHIC4 / SCREEN 5): VRAM holds four 32 KB pages of 256 lines each. The VDP
// scans out ONE page, selected by R#2. The command engine addresses a page by its Y range
// (page p == command Y in [p*256 .. p*256+255]). Double buffering shows one page while you
// draw the next frame into the hidden one, then flips — eliminating erase/redraw flicker.
//
// Typical frame, paired with the vsync extension for a tear-free flip:
//     u16 by = Display_GetBackPageY();          // Y offset of the hidden page
//     ... draw this frame with command DY += by ...
//     VDP_WaitVBlank();                          // vsync ext — sync to the safe window
//     Display_Flip();                            // reveal what we drew; hide the old page
#ifndef MSXMVL_EXT_DISPLAY_H
#define MSXMVL_EXT_DISPLAY_H

#include "types.h"

// Reset to the initial double-buffer state: show page 0, draw into page 1.
void Display_InitDoubleBuffer(void);

// Low-level: make the VDP scan out `page` (0..3) by writing R#2. Does not touch the
// front/back bookkeeping — Init/Flip use it internally; call it directly only for
// manual page control.
void Display_ShowPage(u8 page);

// Show the current back (hidden) page and swap front<->back. Does NOT wait for vblank;
// call VDP_WaitVBlank() first for a tear-free flip.
void Display_Flip(void);

// Page currently scanned out to the screen.
u8 Display_GetFrontPage(void);

// Hidden page — the one you should be drawing into this frame.
u8 Display_GetBackPage(void);

// Y offset (back page * 256) to add to a VDP command's DY so it targets the hidden page.
u16 Display_GetBackPageY(void);

extern u8 g_DispFront;   // page shown on screen
extern u8 g_DispBack;    // page being drawn (hidden)

#endif // MSXMVL_EXT_DISPLAY_H
