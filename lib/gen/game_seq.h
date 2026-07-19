// msxmvl module: game_seq -- data-driven timed sequence / cutscene runner.
//
// A cutscene is data: a list of steps, each an id the caller acts on (show this
// picture, play that sting, move the camera) and a duration in frames. Seq_Update
// ticks one frame and auto-advances when a step's time is up. It is the linear
// timed counterpart to fsm (which is a branching state graph): use fsm for
// "which screen am I on", game_seq for "play these beats in order". A game-framework
// convenience (Category E2); rendering-agnostic and independent of input.
#ifndef MSXMVL_GAME_SEQ_H
#define MSXMVL_GAME_SEQ_H

#include "types.h"

#define SEQ_DONE 0xFF   // Seq_Update / Seq_CurrentId: the sequence has finished

typedef struct {
	u8  id;         // step identifier the caller acts on
	u16 frames;     // how many frames this step lasts (>= 1)
} SeqStep;

typedef struct {
	const SeqStep* steps;
	u8  count;
	u8  index;      // current step
	u16 timer;      // frames elapsed within the current step
	u8  done;       // set once the last step's time is up
} Sequence;

// Set up a sequence over steps[count]; starts at step 0, timer 0.
void Seq_Init(Sequence* s, const SeqStep* steps, u8 count);

// Id of the step active now (SEQ_DONE once the sequence has finished).
u8 Seq_CurrentId(const Sequence* s);

// Advance one frame. Returns the id active *during this frame* (so the caller keeps
// rendering that beat for its full duration), then auto-advances when the step's
// frame count is reached; returns SEQ_DONE once finished. Call once per frame.
u8 Seq_Update(Sequence* s);

// Has the sequence finished?
u8 Seq_IsDone(const Sequence* s);

// Jump to the next step immediately (e.g. a "press START to skip" beat).
void Seq_Skip(Sequence* s);

#endif // MSXMVL_GAME_SEQ_H
