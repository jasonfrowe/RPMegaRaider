#ifndef HUD_H
#define HUD_H

#include <stdint.h>
#include <stdbool.h>

// Set up Mode 1 character config in XRAM and register the text plane.
// Call once during graphics initialisation.
void hud_init(void);

// Zero all text buffer cells (makes text layer transparent).
void hud_clear(void);

// Write a string at character grid position (x, y).
// color is an ANSI palette index (1=red 2=green 3=cyan 4=blue 5=magenta
// 6=yellow 7=white 9=bright-red 10=bright-green 11=bright-cyan 15=bright-white …).
// Background index 0 = transparent.
void hud_draw_text(uint8_t x, uint8_t y, const char *str, uint8_t color);

// Write a string centred on the given row.
void hud_center_text(uint8_t row, const char *str, uint8_t color);

// Score management.
void    hud_add_score(int32_t delta);   // clamps to [0, 999999]
void    hud_reset_score(void);
int32_t hud_get_score(void);

// Render the current score as "000000" at top-centre (row 0, col 17).
// Call every vblank while STATE_PLAYING.
void hud_draw_score(void);

// Render the title screen overlay (call every vblank while STATE_TITLE).
// vsync is RIA.vsync — used for colour cycling and flashing.
void hud_draw_title_screen(uint8_t vsync);

// Render the end screen overlay (call every vblank while STATE_GAMEOVER).
// won == true shows "YOU WIN!", false shows "GAME OVER".
void hud_draw_end_screen(uint8_t vsync, bool won);

#endif // HUD_H
