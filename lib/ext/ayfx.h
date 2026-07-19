// =============================================================================
// msxmvl module: ayfx -- PSG sound-effects layer (Category C1).
//
// This is SapphiRe's ayFX REPLAYER (fixed-volume) as adapted to SDCC by mvac7
// (fR3eL project), integrated into msxmvl -- NOT clean-room. Original MIT-licensed
// sources: <http://www.z80st.es/blog/tags/ayfx> and mvac7's ayFXplayer library.
// Altered for msxmvl (see ayfx.c for the exact changes) and redistributed under the
// original MIT terms with attribution. Full notice: NOTICE.md.
//
// It mixes priority-ranked sound effects INTO msxmvl's PSG shadow (g_PSG_Regs, see
// psg.h): tone/volume/noise land in the shared registers and the ONE PSG_Apply() you
// already call each frame flushes the merged music+SFX state to the chip. Assign the
// SFX its own PSG channel; reserve that channel in your music so they don't fight for
// it (Play uses priorities so a louder effect can pre-empt a quieter one).
// =============================================================================
#ifndef MSXMVL_AYFX_H
#define MSXMVL_AYFX_H

#include "types.h"

// ayFX playback mode (ayFX_Setup).
#define ayFX_FIXED  0   // always play on the channel given to ayFX_Setup
#define ayFX_CYCLE  1   // rotate the SFX across channels each frame

// ayFX_Play return codes.
#define ayFX_OK         0   // effect started
#define ayFX_ERR_PRIO   1   // a higher-priority effect is already playing
#define ayFX_ERR_INDEX  2   // no effect with that index in the bank

// ayFX runtime state (exposed like the original library).
extern u8 ayFX_MODE;        // 0 fixed, 1 cyclic
extern u8 ayFX_CHANNEL;     // internal channel counter (1=C, 2=B, 3=A)
extern u8 ayFX_PRIORITY;    // priority of the effect currently playing (255 = none)

// Set up the replayer over an ayFX sample bank.
//   bank    : address of the ayFX bank (first byte = sample count; then an offset
//             table; then the streams)
//   mode    : ayFX_FIXED or ayFX_CYCLE
//   channel : PSG channel to play on (PSG_CHANNEL_A/B/C, i.e. 0/1/2)
// Does NOT clear the PSG shadow (music keeps playing); only arms the replayer.
void ayFX_Setup(u16 bank, u8 mode, u8 channel) __sdcccall(0);

// Change the output channel (meaningful in fixed mode). channel = 0/1/2 (A/B/C).
void ayFX_SetChannel(u8 channel) __sdcccall(0);

// Start effect `fx` (0..255) at `priority` (0 = highest .. 15 = lowest). Returns one
// of the ayFX_* codes above. A new effect only starts if its priority is at least as
// high as the one currently playing.
u8 ayFX_Play(u8 fx, u8 priority) __sdcccall(0);

// Advance the current effect by one frame, mixing it into g_PSG_Regs. Call once per
// frame (not necessarily in the ISR), before your PSG_Apply(). No-op when idle.
void ayFX_Decode(void);

#endif // MSXMVL_AYFX_H
