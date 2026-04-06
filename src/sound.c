#include <stdint.h>
#include <stdbool.h>
#include "instruments.h"
#include "opl.h"
#include "sound.h"

// ---------------------------------------------------------------------------
// Channel assignments
// ---------------------------------------------------------------------------
#define SFX_JUMP_CH         5
#define SFX_GROUND_ENEMY_CH 6
#define SFX_GHOST_CH        7
#define SFX_PLAYER_CH       8

// ---------------------------------------------------------------------------
// OPL patches
// ---------------------------------------------------------------------------

// Jump: short ascending chirp — bright sine, fast attack/decay (capture-style)
static const OPL_Patch jump_patch = {
    .m_ave=0x61, .m_ksl=0x11, .m_atdec=0xF9, .m_susrel=0x0F, .m_wave=0x01,
    .c_ave=0x61, .c_ksl=0x00, .c_atdec=0xF7, .c_susrel=0x0F, .c_wave=0x00,
    .feedback=0x04,
};

// Charge pack: bright sine arpeggio (lowest pickup priority)
static const OPL_Patch charge_patch = {
    .m_ave=0x21, .m_ksl=0x00, .m_atdec=0xF8, .m_susrel=0x0A, .m_wave=0x00,
    .c_ave=0x21, .c_ksl=0x00, .c_atdec=0xF6, .c_susrel=0x08, .c_wave=0x00,
    .feedback=0x02,
};

// Memory shard: square wave — distinct "chime" timbre vs charge
static const OPL_Patch shard_patch = {
    .m_ave=0x21, .m_ksl=0x00, .m_atdec=0xFF, .m_susrel=0x0F, .m_wave=0x00,
    .c_ave=0x21, .c_ksl=0x00, .c_atdec=0xF9, .c_susrel=0x0A, .c_wave=0x01,
    .feedback=0x04,
};

// Terminus: rich fanfare — full FM feedback
static const OPL_Patch terminus_patch = {
    .m_ave=0x61, .m_ksl=0x11, .m_atdec=0xF5, .m_susrel=0xF8, .m_wave=0x00,
    .c_ave=0x61, .c_ksl=0x00, .c_atdec=0xF3, .c_susrel=0xF6, .c_wave=0x00,
    .feedback=0x0E,
};

// Shield hit: descending harsh impact
static const OPL_Patch impact_patch = {
    .m_ave=0x11, .m_ksl=0x14, .m_atdec=0xF4, .m_susrel=0xC8, .m_wave=0x00,
    .c_ave=0x21, .c_ksl=0x00, .c_atdec=0xF3, .c_susrel=0xC6, .c_wave=0x00,
    .feedback=0x0E,
};

// RUSHER ambient: buzzy drone (borrowed from Cosmic Arc descent_patch)
static const OPL_Patch rusher_patch = {
    .m_ave=0x31, .m_ksl=0x24, .m_atdec=0xF2, .m_susrel=0xF6, .m_wave=0x00,
    .c_ave=0x21, .c_ksl=0x00, .c_atdec=0xE2, .c_susrel=0xF4, .c_wave=0x00,
    .feedback=0x0E,
};

// TRACKER ambient: deep slow sine pulse
static const OPL_Patch tracker_patch = {
    .m_ave=0x21, .m_ksl=0x00, .m_atdec=0xF4, .m_susrel=0xA0, .m_wave=0x00,
    .c_ave=0x21, .c_ksl=0x00, .c_atdec=0xC4, .c_susrel=0x80, .c_wave=0x00,
    .feedback=0x00,
};

// GHOST ambient: wavering half-sine eeriness
static const OPL_Patch ghost_patch = {
    .m_ave=0x21, .m_ksl=0x00, .m_atdec=0xFF, .m_susrel=0x0F, .m_wave=0x02,
    .c_ave=0x21, .c_ksl=0x00, .c_atdec=0xFF, .c_susrel=0x0F, .c_wave=0x02,
    .feedback=0x06,
};

// ---------------------------------------------------------------------------
// Jump sequencer (ch 5)
// ---------------------------------------------------------------------------
#define JUMP_NOTES      3u
#define JUMP_NOTE_TICKS 5u
static const uint8_t jump_notes[JUMP_NOTES] = { 72, 78, 84 };

static bool    jump_active;
static uint8_t jump_note_idx;
static uint8_t jump_tick;

// ---------------------------------------------------------------------------
// Player SFX sequencer (ch 8)
// Priority: 4=shield_hit  3=terminus  2=shard  1=charge  0=idle
// ---------------------------------------------------------------------------
#define CHARGE_NOTES      3u
#define CHARGE_NOTE_TICKS 5u
static const uint8_t charge_notes[CHARGE_NOTES] = { 65, 69, 72 };

#define SHARD_NOTES      4u
#define SHARD_NOTE_TICKS 4u
static const uint8_t shard_notes[SHARD_NOTES] = { 65, 69, 72, 76 };

#define TERMINUS_NOTES      5u
#define TERMINUS_NOTE_TICKS 6u
static const uint8_t terminus_notes[TERMINUS_NOTES] = { 60, 64, 67, 71, 76 };

#define SHIELD_HIT_NOTES      2u
#define SHIELD_HIT_NOTE_TICKS 8u
static const uint8_t shield_hit_notes[SHIELD_HIT_NOTES] = { 55, 43 };

static bool            player_sfx_active;
static uint8_t         player_sfx_prio;
static uint8_t         player_sfx_note_idx;
static uint8_t         player_sfx_tick;
static uint8_t         player_sfx_total_notes;
static uint8_t         player_sfx_note_ticks;
static uint8_t         player_sfx_volume;
static const OPL_Patch *player_sfx_patch;
static const uint8_t   *player_sfx_notes;

// ---------------------------------------------------------------------------
// Ground + ghost enemy ambient (ch 6, ch 7)
// ---------------------------------------------------------------------------
#define RUSHER_RETRIGGER_TICKS  8u
#define TRACKER_RETRIGGER_TICKS 20u
#define GHOST_RETRIGGER_TICKS   6u

static uint8_t ground_enemy_tick;
static uint8_t ghost_tick;
static uint8_t ghost_phase;
static uint8_t prev_ground_mask;  // detect when enemies leave screen

// ---------------------------------------------------------------------------
// Helpers (same pattern as Cosmic Arc sound.c)
// ---------------------------------------------------------------------------
static void sfx_note_on(uint8_t ch, const OPL_Patch *p, uint8_t note, uint8_t vol)
{
    OPL_NoteOff(ch);
    OPL_SetPatch(ch, p);
    OPL_SetVolume(ch, vol);
    OPL_NoteOn(ch, note);
}

static void stop_channel(uint8_t ch)
{
    OPL_NoteOff(ch);
    OPL_SetVolume(ch, 0);
}

// ---------------------------------------------------------------------------
// Internal: start an arbitrary player SFX
// ---------------------------------------------------------------------------
static void start_player_sfx(uint8_t prio, const OPL_Patch *patch,
                              const uint8_t *notes, uint8_t num,
                              uint8_t ticks, uint8_t vol)
{
    player_sfx_active      = true;
    player_sfx_prio        = prio;
    player_sfx_note_idx    = 0;
    player_sfx_tick        = 0;
    player_sfx_total_notes = num;
    player_sfx_note_ticks  = ticks;
    player_sfx_volume      = vol;
    player_sfx_patch       = patch;
    player_sfx_notes       = notes;
    sfx_note_on(SFX_PLAYER_CH, patch, notes[0], vol);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void sound_init(void)
{
    jump_active      = false;
    jump_note_idx    = 0;
    jump_tick        = 0;

    player_sfx_active      = false;
    player_sfx_prio        = 0;
    player_sfx_note_idx    = 0;
    player_sfx_tick        = 0;
    player_sfx_total_notes = 0;
    player_sfx_note_ticks  = 0;
    player_sfx_volume      = 0;
    player_sfx_patch       = 0;
    player_sfx_notes       = 0;

    ground_enemy_tick = 0;
    ghost_tick        = 0;
    ghost_phase       = 0;
    prev_ground_mask  = 0;

    stop_channel(SFX_JUMP_CH);
    stop_channel(SFX_GROUND_ENEMY_CH);
    stop_channel(SFX_GHOST_CH);
    stop_channel(SFX_PLAYER_CH);
}

void sound_play_jump(void)
{
    jump_active   = true;
    jump_note_idx = 0;
    jump_tick     = 0;
    sfx_note_on(SFX_JUMP_CH, &jump_patch, jump_notes[0], 90);
}

void sound_play_pickup_charge(void)
{
    if (player_sfx_active && player_sfx_prio >= 2u) return;
    start_player_sfx(1, &charge_patch, charge_notes, CHARGE_NOTES, CHARGE_NOTE_TICKS, 90);
}

void sound_play_pickup_shard(void)
{
    if (player_sfx_active && player_sfx_prio >= 3u) return;
    start_player_sfx(2, &shard_patch, shard_notes, SHARD_NOTES, SHARD_NOTE_TICKS, 95);
}

void sound_play_terminus(void)
{
    if (player_sfx_active && player_sfx_prio >= 4u) return;
    start_player_sfx(3, &terminus_patch, terminus_notes, TERMINUS_NOTES, TERMINUS_NOTE_TICKS, 110);
}

void sound_play_shield_hit(void)
{
    // Always interrupts — highest priority
    start_player_sfx(4, &impact_patch, shield_hit_notes, SHIELD_HIT_NOTES, SHIELD_HIT_NOTE_TICKS, 127);
}

void sound_update(uint8_t enemy_mask)
{
    // --- Jump sequencer (ch 5) ---
    if (jump_active) {
        jump_tick++;
        if (jump_tick >= JUMP_NOTE_TICKS) {
            jump_tick = 0;
            jump_note_idx++;
            if (jump_note_idx >= JUMP_NOTES) {
                jump_active = false;
                stop_channel(SFX_JUMP_CH);
            } else {
                sfx_note_on(SFX_JUMP_CH, &jump_patch, jump_notes[jump_note_idx], 90);
            }
        }
    }

    // --- Player SFX sequencer (ch 8) ---
    if (player_sfx_active) {
        player_sfx_tick++;
        if (player_sfx_tick >= player_sfx_note_ticks) {
            player_sfx_tick = 0;
            player_sfx_note_idx++;
            if (player_sfx_note_idx >= player_sfx_total_notes) {
                player_sfx_active = false;
                player_sfx_prio   = 0;
                stop_channel(SFX_PLAYER_CH);
            } else {
                sfx_note_on(SFX_PLAYER_CH, player_sfx_patch,
                            player_sfx_notes[player_sfx_note_idx], player_sfx_volume);
            }
        }
    }

    // --- Ground enemy ambient (ch 6): TRACKER > RUSHER ---
    bool has_tracker = (enemy_mask & 0x02u) != 0;
    bool has_rusher  = (enemy_mask & 0x01u) != 0;
    bool has_ground  = has_tracker || has_rusher;
    bool had_ground  = (prev_ground_mask & 0x03u) != 0;

    if (has_ground) {
        ground_enemy_tick++;
        uint8_t retrigger = has_tracker ? TRACKER_RETRIGGER_TICKS : RUSHER_RETRIGGER_TICKS;
        if (ground_enemy_tick >= retrigger) {
            ground_enemy_tick = 0;
            if (has_tracker) {
                sfx_note_on(SFX_GROUND_ENEMY_CH, &tracker_patch, 36, 70);
            } else {
                sfx_note_on(SFX_GROUND_ENEMY_CH, &rusher_patch, 84, 60);
            }
        }
    } else if (had_ground) {
        // Enemies just left the screen — stop the channel
        stop_channel(SFX_GROUND_ENEMY_CH);
        ground_enemy_tick = 0;
    }

    // --- GHOST ambient (ch 7) ---
    bool has_ghost  = (enemy_mask & 0x04u) != 0;
    bool had_ghost  = (prev_ground_mask & 0x04u) != 0;

    if (has_ghost) {
        ghost_tick++;
        if (ghost_tick >= GHOST_RETRIGGER_TICKS) {
            ghost_tick  = 0;
            ghost_phase ^= 1u;
            sfx_note_on(SFX_GHOST_CH, &ghost_patch, ghost_phase ? 62u : 58u, 65);
        }
    } else if (had_ghost) {
        // Ghost just left the screen — stop the channel
        stop_channel(SFX_GHOST_CH);
        ghost_tick  = 0;
        ghost_phase = 0;
    }

    prev_ground_mask = enemy_mask;
}
