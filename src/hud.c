#include <rp6502.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "constants.h"
#include "hud.h"

// ---------------------------------------------------------------------------
// Score state
// ---------------------------------------------------------------------------
static int32_t s_score = 0;

void hud_reset_score(void) { s_score = 0; }
int32_t hud_get_score(void) { return s_score; }

void hud_add_score(int32_t delta)
{
    s_score += delta;
    if (s_score < 0)      s_score = 0;
    if (s_score > 999999) s_score = 999999;
}

// ---------------------------------------------------------------------------
// Low-level XRAM write: one text cell = { glyph, fg_index, bg_index }
// bg_index 0 = transparent (tile layers show through).
// ---------------------------------------------------------------------------
static void write_cell(unsigned addr, uint8_t glyph, uint8_t fg)
{
    RIA.addr0 = addr;
    RIA.step0 = 1;
    RIA.rw0 = glyph;
    RIA.rw0 = fg;
    RIA.rw0 = 0;    // transparent background
}

// Write a run of transparent (blank) cells starting at addr.
static void clear_cells(unsigned addr, uint8_t count)
{
    RIA.addr0 = addr;
    RIA.step0 = 1;
    uint8_t i;
    for (i = 0; i < count; i++) {
        RIA.rw0 = 0;
        RIA.rw0 = 0;
        RIA.rw0 = 0;
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void hud_init(void)
{
    // Write vga_mode1_config_t into XRAM at TEXT_MODE1_CFG.
    xram0_struct_set(TEXT_MODE1_CFG, vga_mode1_config_t, x_wrap,        false);
    xram0_struct_set(TEXT_MODE1_CFG, vga_mode1_config_t, y_wrap,        false);
    xram0_struct_set(TEXT_MODE1_CFG, vga_mode1_config_t, x_pos_px,      0);
    xram0_struct_set(TEXT_MODE1_CFG, vga_mode1_config_t, y_pos_px,      0);
    xram0_struct_set(TEXT_MODE1_CFG, vga_mode1_config_t, width_chars,   TEXT_W);
    xram0_struct_set(TEXT_MODE1_CFG, vga_mode1_config_t, height_chars,  TEXT_H);
    xram0_struct_set(TEXT_MODE1_CFG, vga_mode1_config_t, xram_data_ptr, TEXT_DATA_BASE);
    xram0_struct_set(TEXT_MODE1_CFG, vga_mode1_config_t, xram_palette_ptr, 0xFFFF); // system palette
    xram0_struct_set(TEXT_MODE1_CFG, vga_mode1_config_t, xram_font_ptr,    0xFFFF); // system CP437 font

    // Mode 1 (character), options=3 (8-bit color), config at TEXT_MODE1_CFG, plane 2
    xregn(1, 0, 1, 4, 1, 3, TEXT_MODE1_CFG, 2);

    hud_clear();
}

void hud_clear(void)
{
    unsigned addr = TEXT_DATA_BASE;
    unsigned total = (unsigned)TEXT_W * (unsigned)TEXT_H * 3u;
    RIA.addr0 = addr;
    RIA.step0 = 1;
    unsigned i;
    for (i = 0; i < total; i++)
        RIA.rw0 = 0;
}

void hud_draw_text(uint8_t x, uint8_t y, const char *str, uint8_t color)
{
    unsigned addr = TEXT_DATA_BASE + ((unsigned)y * TEXT_W + (unsigned)x) * 3u;
    RIA.addr0 = addr;
    RIA.step0 = 1;
    while (*str) {
        RIA.rw0 = (uint8_t)*str++;
        RIA.rw0 = color;
        RIA.rw0 = 0;
    }
}

void hud_center_text(uint8_t row, const char *str, uint8_t color)
{
    uint8_t len = (uint8_t)strlen(str);
    uint8_t x   = (TEXT_W > len) ? (uint8_t)((TEXT_W - len) / 2u) : 0u;
    hud_draw_text(x, row, str, color);
}

// ---------------------------------------------------------------------------
// Score display — row 0, columns 17-22 ("000000", 6 digits)
// ---------------------------------------------------------------------------
void hud_draw_score(void)
{
    // Format 6-digit zero-padded decimal into a 7-byte buffer (+ null)
    char buf[7];
    int32_t v = s_score;
    buf[6] = '\0';
    buf[5] = (char)('0' + (v % 10)); v /= 10;
    buf[4] = (char)('0' + (v % 10)); v /= 10;
    buf[3] = (char)('0' + (v % 10)); v /= 10;
    buf[2] = (char)('0' + (v % 10)); v /= 10;
    buf[1] = (char)('0' + (v % 10)); v /= 10;
    buf[0] = (char)('0' + (v % 10));
    // col 17 centres 6 chars in 40 columns
    hud_draw_text(17, 0, buf, 11); // bright cyan
}

// ---------------------------------------------------------------------------
// Title screen
// ---------------------------------------------------------------------------
static const uint8_t s_cycle_colors[6] = { 11, 14, 10, 9, 13, 12 };
//    bright-cyan, bright-yellow, bright-green, bright-red, bright-magenta, bright-blue

void hud_draw_title_screen(uint8_t vsync)
{
    uint8_t color = s_cycle_colors[(vsync / 8u) % 6u];

    // Draw title every frame (color cycles)
    hud_draw_text(10,  18, "M E G A   R A I D E R", color);

    // Subtitle — dim white
    hud_draw_text(10,  21, "ESCAPE THE DATA VAULT", 7);

    // Controls hint — grey
    hud_draw_text(8,  23, "COLLECT SHARDS REACH EXIT", 6);
    hud_draw_text(8,  24, "SHIELD ABSORBS ENEMY HITS", 6);

    // Flashing PRESS START — bright white when on
    if ((vsync / 30u) & 1u) {
        hud_draw_text(14, 26, "PRESS  START", 15);
    } else {
        clear_cells(TEXT_DATA_BASE + (26u * TEXT_W + 14u) * 3u, 12);
    }
}

// ---------------------------------------------------------------------------
// End screen (game over / you win)
// ---------------------------------------------------------------------------
void hud_draw_end_screen(uint8_t vsync, bool won)
{
    if (won) {
        hud_draw_text(12, 10, "Y O U   W I N !", 10); // bright green
    } else {
        hud_draw_text(11, 10, "G A M E   O V E R", 9); // bright red
    }

    // Final score label
    hud_draw_text(14, 17, "FINAL  SCORE", 7);

    // Score value (6 digits, same formatting as hud_draw_score)
    {
        char buf[7];
        int32_t v = s_score;
        buf[6] = '\0';
        buf[5] = (char)('0' + (v % 10)); v /= 10;
        buf[4] = (char)('0' + (v % 10)); v /= 10;
        buf[3] = (char)('0' + (v % 10)); v /= 10;
        buf[2] = (char)('0' + (v % 10)); v /= 10;
        buf[1] = (char)('0' + (v % 10)); v /= 10;
        buf[0] = (char)('0' + (v % 10));
        hud_draw_text(17, 19, buf, 11); // bright cyan
    }

    // Flashing PRESS START
    if ((vsync / 30u) & 1u) {
        hud_draw_text(14, 22, "PRESS  START", 15);
    } else {
        clear_cells(TEXT_DATA_BASE + (22u * TEXT_W + 14u) * 3u, 12);
    }
}
