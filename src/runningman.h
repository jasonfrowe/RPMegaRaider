#ifndef RUNNINGMAN_H
#define RUNNINGMAN_H

#include <stdint.h>

// Set by maze_generate() before runningman_init() is called
extern int16_t player_start_x;
extern int16_t player_start_y;

void runningman_init(void);
void runningman_update(void);
int16_t runningman_get_x(void);
int16_t runningman_get_y(void);

#endif // RUNNINGMAN_H
