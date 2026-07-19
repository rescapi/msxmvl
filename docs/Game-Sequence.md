# Game Sequence (`game_seq`)

A **timed sequence runner** for intros, cutscenes, attract loops, scripted beats — the
linear counterpart to [`fsm`](State-Machines.md). Where `fsm` answers *"which screen am
I on"* (a branching graph), `game_seq` answers *"play these beats in order"* (a straight
line on a timer). You describe the sequence as data — an array of `{id, frames}` steps —
and each frame `Seq_Update` hands back the beat to render and auto-advances when a beat's
time is up. It is **rendering-agnostic**: you act on the returned `id` however you like.
Link `game_seq.c`, include `game_seq.h`.

## API

```c
#include "game_seq.h"

void Seq_Init(Sequence* s, const SeqStep* steps, u8 count); // start at step 0
u8   Seq_CurrentId(const Sequence* s);   // id of the active beat (SEQ_DONE if finished)
u8   Seq_Update(Sequence* s);            // tick one frame; returns the beat active now
u8   Seq_IsDone(const Sequence* s);      // has the sequence finished?
void Seq_Skip(Sequence* s);              // jump to the next beat now (e.g. START to skip)
```

## Usage

```c
static const SeqStep g_Intro[3] = {
	{ BEAT_LOGO,  120 },     // show the logo for 120 frames (~2.4s @ 50Hz)
	{ BEAT_TITLE,  90 },     // then the title for 90 frames
	{ BEAT_PRESS,  60 },     // then "press start" for 60 frames
};
Sequence g_Seq;

void intro(void) {
	Seq_Init(&g_Seq, g_Intro, 3);
	while (!Seq_IsDone(&g_Seq)) {
		VDP_WaitVBlank();                // one tick per frame
		u8 beat = Seq_Update(&g_Seq);    // the beat to render this frame
		draw_beat(beat);                 // your renderer
		if (Joystick_IsButtonPushed(JOY_PORT_1, JOY_INPUT_TRIGGER_A))
			Seq_Skip(&g_Seq);           // let the player skip ahead
	}
}
```

`Seq_Update` returns the id **active during this frame** — so a beat with `frames = N`
is shown for exactly `N` frames — then advances, returning `SEQ_DONE` once the last
beat's time is up. A `frames` value of at least 1 is expected.

## Correctness

`game_seq_01.c` ticks a three-beat sequence (`{1,2}`, `{2,1}`, `{3,3}`) and asserts the
exact frame-by-frame timeline `1,1,2,3,3,3,DONE`, plus the `Seq_Skip` walk
`1→2→3→DONE` — deterministically, and passes on C-BIOS MSX2.
*(tested: `game_seq_01.c`)*

## See also
- [State Machines](State-Machines.md) — `fsm` for branching screen/scene flow.
- [Game Menu](Game-Menu.md) — `game_menu` for the interactive screens between beats.
- [VBlank Sync](VBlank-Sync.md) — the per-frame tick `Seq_Update` is called on.
