#ifndef STREAM_H
#define STREAM_H

#include <stdint.h>

// Open MAZE_FG.BIN and MAZE_BG.BIN from the USB drive.
// Returns 0 on success, -1 on error.
int stream_open_files(void);

// Pre-fill the FG and BG ring buffers for the initial camera position.
// cam_x_px / cam_y_px are the top-left camera pixel coordinates (world-space).
void stream_init(int16_t cam_x_px, int16_t cam_y_px);

// Two-phase streaming to eliminate screen tearing:
//
//   stream_prefetch()  — call repeatedly during the vsync spin-wait.
//                        Reads USB maze data into a CPU-side staging buffer.
//                        Safe during active scan (no XRAM writes).
//
//   stream_commit()    — call ONCE, immediately after vsync fires (vblank).
//                        Flushes the staging buffer into the XRAM ring buffers.
//                        All XRAM writes are confined to the vblank window.
void stream_prefetch(int16_t cam_x_px, int16_t cam_y_px);
void stream_commit(void);

// Close the maze files.
void stream_close_files(void);

// Read the FG tile index for world tile (wx, wy) from the XRAM ring buffer.
// Only valid for tiles that are currently within the loaded window.
uint8_t stream_read_fg_tile(uint16_t wx, uint16_t wy);

// Current ring buffer window position (for diagnostics).
uint16_t stream_get_loaded_left(void);
uint16_t stream_get_loaded_top(void);

#endif // STREAM_H
