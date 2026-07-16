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

## A hero: idle → jump

The player's own behaviour is a natural state machine. Here the hero stands idle (an animation
ticking over every frame), then jumps: entering the jump runs its `Begin`, leaving idle runs its
`End`. Each state is a struct of callbacks; a few thin wrappers give the rest of the game a verb
per action.

```c
// hero_fsm.c — the player's own state machine: stand idle, then jump.
#include "fsm.h"

static u8 g_IdleFrames;    // idle animation cursor: frames spent standing
static u8 g_JumpsStarted;  // how many times the hero launched a jump
static u8 g_IdleExits;     // how many times the hero left the idle state

static void idle_update(void) { g_IdleFrames++; }    // advance the idle animation
static void idle_leave(void)  { g_IdleExits++; }     // clean up as we leave idle
static void jump_launch(void) { g_JumpsStarted++; }  // kick off the jump arc
static void jump_update(void) { }                    // airborne this frame

//                         Flag  Begin        Update       End
static FSM_State g_Idle = {  0,  NULL,        idle_update, idle_leave };
static FSM_State g_Jump = {  0,  jump_launch, jump_update, NULL       };

void hero_init(void)       { FSM_SetState(&g_Idle); }
void hero_jump(void)       { FSM_SetState(&g_Jump); }
void hero_tick(void)       { FSM_Update(); }
bool hero_is_jumping(void) { return FSM_GetState() == &g_Jump; }
```

`hero_init()` queues the idle state; the first `hero_tick()` applies it and then ticks it, so two
ticks leave `g_IdleFrames == 2`. `hero_jump()` queues the jump, and the next `hero_tick()` runs
`idle_leave`, then `jump_launch`, then `jump_update` — after which `hero_is_jumping()` is true.
Note `Begin`/`End` are `NULL` where a state has no enter/leave work; the module guards against that.
*(tested: `fsm_01_states.c`)*

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
