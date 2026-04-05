#ifndef RUNNINGMAN_H
#define RUNNINGMAN_H

#include <stdint.h>
#include <stdbool.h>

// Set by maze_generate() before runningman_init() is called
extern int16_t player_start_x;
extern int16_t player_start_y;

void    runningman_init(void);
void    runningman_update(void);
int16_t runningman_get_x(void);
int16_t runningman_get_y(void);
uint8_t runningman_get_charge(void);
uint8_t runningman_get_shards(void);
uint8_t runningman_get_lives(void);
bool    runningman_is_alive(void);
void    runningman_flush_tile_writes(void);

#endif // RUNNINGMAN_H
