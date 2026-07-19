# Game State (`game_state`)

Two small state-plumbing helpers every game reinvents:

- **State stack** — a LIFO of state ids. Push *pause* on top of *play* and pop back to
  exactly where you were; nest a menu over a map over a title. It complements
  [`fsm`](State-Machines.md) (which tracks one flat current state) when you need a
  back-stack.
- **Packed flags** — boolean flags 8-per-byte, so 256 progress/switch flags fit in 32
  bytes — cheap to keep in an [SRAM save](Game-Saves.md). Plain functions over a byte
  array you own; no global state.

Link `game_state.c`, include `game_state.h`.

## API

```c
#include "game_state.h"

// State stack (depth STATE_STACK_MAX = 8)
void StateStack_Init(StateStack* s);
u8   StateStack_Push(StateStack* s, u8 state);  // 1 ok, 0 if full
u8   StateStack_Pop(StateStack* s);             // popped id, or STATE_NONE if empty
u8   StateStack_Top(const StateStack* s);       // peek, or STATE_NONE
u8   StateStack_Count(const StateStack* s);

// Packed boolean flags (index is the bit number across the array)
void Flags_Clear(u8* flags, u8 nbytes);
void Flags_Set(u8* flags, u16 index);
void Flags_Reset(u8* flags, u16 index);
void Flags_Toggle(u8* flags, u16 index);
u8   Flags_Get(const u8* flags, u16 index);     // 1 if set, else 0
```

## Usage

```c
StateStack g_States;
u8 g_Progress[32];               // 256 progress flags

void enter_pause(void) {
	StateStack_Push(&g_States, ST_PAUSE);   // ST_PLAY stays underneath
}
void leave_pause(void) {
	StateStack_Pop(&g_States);              // back to whatever we paused
	u8 now = StateStack_Top(&g_States);     // == ST_PLAY
}

void on_boss_defeated(void) {
	Flags_Set(g_Progress, FLAG_BOSS1);      // remember it; save g_Progress to SRAM
}
if (Flags_Get(g_Progress, FLAG_BOSS1)) { /* door is open */ }
```

`STATE_NONE` (0xFF) is returned by `Pop`/`Top` on an empty stack, so a game loop can
`while ((s = StateStack_Top(&g_States)) != STATE_NONE)` drive the current screen.

## Correctness

`game_state_01.c` exercises both deterministically — the push/pop order
(`MENU/PLAY/PAUSE` down to empty → `STATE_NONE`), the full-stack rejection (9th push
fails), and packed flags across byte boundaries (set bits 0/7/8/31, toggle, reset) —
and passes on C-BIOS MSX2. *(tested: `game_state_01.c`)*

## See also
- [State Machines](State-Machines.md) — `fsm`, the flat current-state counterpart.
- [Game Sequence](Game-Sequence.md) — `game_seq` for timed beats.
- [Game Saves](Game-Saves.md) — persist a packed-flags array to battery-backed SRAM.
