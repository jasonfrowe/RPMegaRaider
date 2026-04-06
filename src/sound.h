#ifndef SOUND_H
#define SOUND_H

#include <stdint.h>

// ---------------------------------------------------------------------------
// Channel layout (music uses 0-4, SFX uses 5-8)
//
//   5 — Player jump (dedicated, never interrupted)
//   6 — Ground enemies: TRACKER > RUSHER priority (continuous ambient)
//   7 — GHOST enemy  (dedicated continuous ambient)
//   8 — Player impact/pickup: shield_hit > terminus > shard > charge
// ---------------------------------------------------------------------------

// Initialise all SFX state and silence SFX channels. Call after opl_init().
void sound_init(void);

// Advance SFX sequencers by one frame. Call once per frame (VBLANK).
// enemy_mask: bit 0 = RUSHER on screen, bit 1 = TRACKER, bit 2 = GHOST.
// Pass 0 outside STATE_PLAYING to silence enemy channels.
void sound_update(uint8_t enemy_mask);

// One-shot player SFX triggers (call from game logic, not the update loop).
void sound_play_jump(void);           // ch 5: 3-note ascending chirp
void sound_play_shield_hit(void);     // ch 8: 2-note descending impact (highest prio)
void sound_play_pickup_charge(void);  // ch 8: 3-note ascending arpeggio (lowest prio)
void sound_play_pickup_shard(void);   // ch 8: 4-note ascending (medium prio)
void sound_play_terminus(void);       // ch 8: 5-note fanfare (high prio)

#endif // SOUND_H
