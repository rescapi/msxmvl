// intro_cutscene.c -- a timed intro sequence: three beats, then done.
//
// A cutscene is data: {id, frames} steps. Each frame you call Seq_Update(&seq) and
// render the beat it returns; it auto-advances when a beat's time is up and returns
// SEQ_DONE at the end. Here we tick it deterministically and check the exact beat
// timeline, plus the "skip" control, so the behaviour is self-verifying.
#include "game_seq.h"

// id 1 for 2 frames, id 2 for 1 frame, id 3 for 3 frames -> 6 frames total.
static const SeqStep g_Intro[3] = {
	{ 1, 2 },
	{ 2, 1 },
	{ 3, 3 },
};

// ---- test harness --------------------------------------------------------
volatile u8 __at(0xE000) R[8];
void main(void)
{
	Sequence seq;
	u8 i, ok;
	// Expected id returned by Seq_Update on frames 1..7 (7th is past the end).
	static const u8 expect[7] = { 1, 1, 2, 3, 3, 3, SEQ_DONE };

	Seq_Init(&seq, g_Intro, 3);
	ok = 1;
	for (i = 0; i < 7; i++)
	{
		u8 got = Seq_Update(&seq);
		R[1 + i] = got;                 // record the beat timeline in R[1..7]
		if (got != expect[i])
			ok = 0;
	}
	if (!Seq_IsDone(&seq))              // must be finished after the timeline
		ok = 0;

	// Skip control: from a fresh sequence, three skips walk 1 -> 2 -> 3 -> done.
	Seq_Init(&seq, g_Intro, 3);
	if (Seq_CurrentId(&seq) != 1) ok = 0;
	Seq_Skip(&seq);
	if (Seq_CurrentId(&seq) != 2) ok = 0;
	Seq_Skip(&seq);
	if (Seq_CurrentId(&seq) != 3) ok = 0;
	Seq_Skip(&seq);
	if (Seq_CurrentId(&seq) != SEQ_DONE || !Seq_IsDone(&seq)) ok = 0;

	R[0] = ok ? 0xA5 : 0x00;
	for (;;) {}
}
