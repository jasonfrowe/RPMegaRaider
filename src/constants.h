#ifndef CONSTANTS_H
#define CONSTANTS_H

// Screen dimensions
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define SCREEN_HALF_WIDTH (SCREEN_WIDTH / 2)
#define SCREEN_HALF_HEIGHT (SCREEN_HEIGHT / 2)

// Sprite data configuration
#define SPRITE_DATA_START        0x0000U // Starting address in XRAM for sprite data

#define RUNNING_MAN_DATA        (SPRITE_DATA_START) // Address for main tile bitmap data
#define RUNNING_MAN_DATA_SIZE    0x2400U // 9216 bytes (18 frames * 16x16 * 16bpp)

#define ENEMY_DATA              (RUNNING_MAN_DATA + RUNNING_MAN_DATA_SIZE) // Address for enemy tile bitmap data
#define ENEMY_DATA_SIZE         0x1000U // 4096 bytes (8 frames * 16x16 * 16bpp)

#define MAIN_MAP_DATA           (ENEMY_DATA + ENEMY_DATA_SIZE) // Address for main tile bitmap data
#define MAIN_MAP_DATA_SIZE      0x4000U // 16384 bytes (256 tiles * 8x8 * 8bpp)

#define MAIN_MAP_TILEMAP_DATA   (MAIN_MAP_DATA + MAIN_MAP_DATA_SIZE) // Address for Main Map tilemap data
#define MAIN_MAP_TILEMAP_SIZE   0x7530U // 30000 bytes (200 * 150 tile IDs)

#define SPRITE_DATA_END         (MAIN_MAP_TILEMAP_DATA + MAIN_MAP_TILEMAP_SIZE) // End address for sprite data

// Main Map configuration
#define MAIN_MAP_WIDTH_TILES 200
#define MAIN_MAP_HEIGHT_TILES 150

// RIA input buffers are provided at fixed XRAM addresses.
#define GAMEPAD_INPUT   0xFF78  // 40 bytes for 4 gamepads
#define KEYBOARD_INPUT  0xFFA0  // 32 bytes keyboard bitfield

// Keyboard, Gamepad and Sound
// -------------------------------------------------------------------------
#define PALETTE_ADDR    0xFC00  // 256-color palette (512 bytes, 0xFC00-0xFDFF)
#define PALETTE_SIZE    0x0200

#define OPL_ADDR        0xFE00  // OPL2 register page (256 bytes, must be page-aligned)
#define OPL_SIZE        0x0100

// RIA input buffers are provided at fixed XRAM addresses.
#define GAMEPAD_INPUT   0xFF78  // 40 bytes for 4 gamepads
#define KEYBOARD_INPUT  0xFFA0  // 32 bytes keyboard bitfield

extern unsigned RUNNING_MAN_CONFIG;

#endif // CONSTANTS_H