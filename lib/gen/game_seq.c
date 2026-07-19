// msxmvl module: game_seq -- data-driven timed sequence runner. See game_seq.h.
#include "game_seq.h"

void Seq_Init(Sequence* s, const SeqStep* steps, u8 count)
{
	s->steps = steps;
	s->count = count;
	s->index = 0;
	s->timer = 0;
	s->done  = (count == 0);
}

u8 Seq_CurrentId(const Sequence* s)
{
	return s->done ? SEQ_DONE : s->steps[s->index].id;
}

u8 Seq_Update(Sequence* s)
{
	u8 id;
	if (s->done)
		return SEQ_DONE;
	id = s->steps[s->index].id;          // active during this frame
	s->timer++;
	if (s->timer >= s->steps[s->index].frames)
	{
		s->timer = 0;
		s->index++;
		if (s->index >= s->count)
			s->done = 1;
	}
	return id;
}

u8 Seq_IsDone(const Sequence* s)
{
	return s->done;
}

void Seq_Skip(Sequence* s)
{
	if (s->done)
		return;
	s->timer = 0;
	s->index++;
	if (s->index >= s->count)
		s->done = 1;
}
