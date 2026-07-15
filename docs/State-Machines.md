# State Machines (`fsm`)

A tiny finite state machine: a game is usually a handful of screens — title, play, game-over — each
with its own per-frame logic and its own enter/leave work. `fsm` gives each state three callbacks
and applies transitions at a safe point. Link `fsm.c`, include `fsm.h`.

## The model

A state is a struct of three optional callbacks:

```c
typedef struct FSM_State
{
	u8       Flag;     // reserved
	callback Begin;    // called once when the state is entered   (may be NULL)
	callback Update;   // called every FSM_Update tick
	callback End;      // called once when the state is left      (may be NULL)
} FSM_State;
```

The key detail: **`FSM_SetState` does not switch immediately** — it *queues* the new state. The
switch happens inside the next `FSM_Update`, which runs `End` on the old state, `Begin` on the new
one, and then the new `Update`. That means it is safe to call `FSM_SetState` from inside an
`Update` callback: the rest of that frame's code still runs with a consistent current state.

(Don't call it from a `Begin`/`End` callback — you'd be queueing a transition from inside a
transition.)

## API

```c
void       FSM_SetState(FSM_State* state);   // queue a transition
void       FSM_Update(void);                 // apply any pending transition, then tick
FSM_State* FSM_GetState(void);               // current state
FSM_State* FSM_GetPrevious(void);            // state we came from
void       FSM_SetPrevious(void);            // queue a return to the previous state
bool       FSM_IsPending(void);              // is a transition queued?
FSM_State* FSM_GetPending(void);             // the queued state
```

## A title → game transition

```c
#include "fsm.h"
volatile u8 __at(0xE000) R[8];

static u8 g_TitleTicks, g_GameEnters, g_TitleLeft;

static void Title_Update(void) { g_TitleTicks++; }
static void Title_End(void)    { g_TitleLeft++; }
static void Game_Begin(void)   { g_GameEnters++; }
static void Game_Update(void)  { }

//                       Flag  Begin       Update        End
static FSM_State g_Title = { 0, NULL,       Title_Update, Title_End };
static FSM_State g_Game  = { 0, Game_Begin, Game_Update,  NULL      };

void main(void)
{
	FSM_SetState(&g_Title);   // queued, not applied yet
	R[1] = FSM_IsPending() ? 1 : 0;             // 1

	FSM_Update();             // applies Title, then ticks it
	FSM_Update();
	R[2] = g_TitleTicks;                        // 2

	FSM_SetState(&g_Game);    // queue the transition
	FSM_Update();             // Title.End, Game.Begin, Game.Update
	R[3] = g_TitleLeft;                         // 1
	R[4] = g_GameEnters;                        // 1
	R[5] = (FSM_GetState() == &g_Game) ? 1 : 0; // 1

	R[0] = (R[1] == 1 && R[2] == 2 && R[3] == 1 && R[4] == 1 && R[5] == 1)
	     ? 0xA5 : 0x00;
	for (;;) {}
}
```

Runs to `R[] = a5 01 02 01 01 01`. Note `Begin`/`End` are `NULL` where the state has no
enter/leave work — the module guards against that. *(tested: `fsm_01_states.c`)*

## The usual game loop

```c
FSM_SetState(&g_Title);

for (;;) {
	VDP_WaitVBlank();
	FSM_Update();      // one tick of whatever state we are in
}
```

`FSM_SetPrevious()` gives you a cheap "back" — handy for a pause screen that returns to whatever
was running:

```c
static void Pause_Update(void)
{
	if (Keyboard_IsKeyPushed(KEY_ESC))
		FSM_SetPrevious();     // queued; applied on the next FSM_Update
}
```

## See also

- [VBlank Sync](VBlank-Sync.md) — pace `FSM_Update` to the frame.
- [Keyboard Input](Keyboard-Input.md) — driving transitions from key presses.
