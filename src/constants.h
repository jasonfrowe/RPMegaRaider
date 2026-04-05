#ifndef CONSTANTS_H
#define CONSTANTS_H

// ---------------------------------------------------------------------------
// Screen
// ---------------------------------------------------------------------------
#define SCREEN_WIDTH        320
#define SCREEN_HEIGHT       240
#define SCREEN_HALF_WIDTH   (SCREEN_WIDTH / 2)
#define SCREEN_HALF_HEIGHT  (SCREEN_HEIGHT / 2)

// ---------------------------------------------------------------------------
// Tile dimensions
// ---------------------------------------------------------------------------
#define TILE_W  8
#define TILE_H  8

// ---------------------------------------------------------------------------
// Streaming ring-buffer dimensions (power-of-2 for cheap modulo via AND)
// RING_W=64: covers 512px horizontally (screen=320px, slack=192px)
// RING_H=32: covers 256px vertically  (screen=240px, slack=16px)
// ---------------------------------------------------------------------------
#define RING_W  64
#define RING_H  32

// ---------------------------------------------------------------------------
// World dimensions
// ---------------------------------------------------------------------------
#define WORLD_W     800     // tiles wide
#define WORLD_H     600     // tiles tall
#define WORLD_W_PX  6400    // pixels wide  (800 * 8, fits in int16_t)
#define WORLD_H_PX  4800    // pixels tall  (600 * 8, fits in int16_t)

// ---------------------------------------------------------------------------
// FG tile IDs (collision logic in runningman.c)
// ---------------------------------------------------------------------------
#define TILE_EMPTY      0
#define TILE_SOLID_MIN  1
#define TILE_SOLID_MAX  30
#define TILE_LADDER_MIN 107
#define TILE_LADDER_MAX 110

// ---------------------------------------------------------------------------
// XRAM Layout
//
//   0x0000  Player sprite     9,216 B  (18 frames × 16×16 × RGB555)
//   0x2400  Enemy sprites     4,096 B  (8 frames × 16×16 × RGB555, reserved)
//   0x3400  FG tileset        8,192 B  (256 tiles × 8×8 × 4bpp)
//   0x5400  BG tileset        8,192 B  (256 tiles × 8×8 × 4bpp)
//   0x7400  FG tilemap        2,048 B  (64×32 ring buffer, 1B/tile)
//   0x7C00  BG tilemap        2,048 B  (64×32 ring buffer, 1B/tile)
//   0x8400  FG palette           32 B  (16 × RGB555 LE)
//   0x8420  BG palette           32 B
//   0x8440  BG Mode2 config  ~32 B B  (vga_mode2_config_t)
//   0x8460  FG Mode2 config      ~32 B
//   0x8480  Sprite config        ~32 B (vga_mode4_sprite_t)
//   0xFF78  Gamepads             40 B  (4 × 10B, system region)
//   0xFFA0  Keyboard             32 B  (HID keycodes bitfield)
// ---------------------------------------------------------------------------

#define PLAYER_SPRITE_BASE  0x0000U
#define PLAYER_SPRITE_SIZE  0x2400U

#define ENEMY_SPRITE_BASE   0x2400U
#define ENEMY_SPRITE_SIZE   0x1000U

#define FG_TILES_BASE       0x3400U
#define FG_TILES_SIZE       0x2000U   // 8192 bytes

#define BG_TILES_BASE       0x5400U
#define BG_TILES_SIZE       0x2000U

#define FG_TILEMAP_BASE     0x7400U
#define FG_TILEMAP_SIZE     (RING_W * RING_H)   // 2048 bytes

#define BG_TILEMAP_BASE     0x7C00U
#define BG_TILEMAP_SIZE     (RING_W * RING_H)

#define FG_PALETTE_BASE     0x8400U
#define BG_PALETTE_BASE     0x8420U

#define BG_MODE2_CFG        0x8440U
#define FG_MODE2_CFG        0x8460U
#define SPRITE_CFG          0x8480U

// ---------------------------------------------------------------------------
// Sprite config slot addressing
//   sizeof(vga_mode4_sprite_t) = 8 bytes on this platform.
//   Slot 0 = player, slots 1-7 = enemies (contiguous, one xreg_vga_mode call).
// ---------------------------------------------------------------------------
#define SPRITE_CFG_SIZE     8u
#define SPRITE_COUNT        8u
#define ENEMY_CFG(n)        (SPRITE_CFG + (unsigned)(n) * SPRITE_CFG_SIZE)

// ---------------------------------------------------------------------------
// Pickup / special FG tile IDs  (above TILE_SOLID_MAX → not solid)
// ---------------------------------------------------------------------------
#define TILE_CHARGE_PACK    31
#define TILE_MEMORY_SHARD   32
#define TILE_TERMINUS       33

// ---------------------------------------------------------------------------
// Gameplay tuning
// ---------------------------------------------------------------------------
#define EMP_RADIUS_PX           80
#define EMP_COOLDOWN_FRAMES     90
#define IMMUNITY_FRAMES         60
#define LIVES_START             3
#define SHARDS_NEEDED           5
#define MAX_ENEMIES             7
#define ENEMY_RESPAWN_FRAMES    300

// Input / system
#define GAMEPAD_INPUT       0xFF78U
#define KEYBOARD_INPUT      0xFFA0U

// Compatibility aliases used by runningman.c
#define RUNNING_MAN_CONFIG  SPRITE_CFG
#define RUNNING_MAN_DATA    PLAYER_SPRITE_BASE

#endif // CONSTANTS_H