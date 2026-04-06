# RPMegaRaider

Escape the data vault in a retro action-platformer built for RP6502 with llvm-mos.

You play as a raider dropped into a giant scrolling maze full of hostile security drones, power pickups, and one very expensive Terminus reward. Grab points, survive the swarm, and leave through the exit portal when you decide the run is done.

## What This Game Is

- 320x240 pixel-art action with smooth camera scrolling in a huge 800x600 tile world.
- Dual tile layers (foreground collision + background parallax) streamed from ROM.
- Sprite-based player and enemy system with shield hits, enemy respawns, and hazard pressure.
- OPL music + SFX tuned for arcade pacing.

## Objective

- Explore and score as much as possible.
- Exit through the portal to end the run on your terms.
- There is no shard requirement to finish.

## Scoring

- Charge Pack: 1~000
- Memory Shard: 10~000
- Terminus pickup: 50~000
- Exit bonus: 23~000
- Shield collision penalty: -250

Perfect-score target is 300~000.

## Controls

Keyboard:

- Arrow Up: climb up ladder
- Arrow Down: climb down ladder
- Arrow Left / Arrow Right: move
- Space: jump
- Enter: Start (title/game-over transitions)

Gamepad:

- Left stick or D-pad: move and ladder climb
- A: jump
- Start: Start (title/game-over transitions)

## Build

Requirements:

- llvm-mos SDK in your PATH
- CMake 3.18+
- Python 3
- VS Code with CMake Tools and RP6502 tooling

Configure and build:

    cmake -S . -B build/target-native -DCMAKE_BUILD_TYPE=Release
    cmake --build build/target-native

Output package:

- build/target-native/RPMegaRaider.rp6502

## Run / Deploy

From VS Code:

- Use Start Debugging (F5) to build and upload to hardware.

From terminal tools:

    python tools/rp6502.py term

If your serial or upload target is different, adjust local RP6502 tool settings for your machine.

## Project Layout

- src/main.c: game loop, state transitions, camera, render orchestration
- src/stream.c: map streaming and ring-buffer commits
- src/runningman.c: player movement, pickups, collision, win/lose
- src/enemy.c: enemy behaviors, activation, drawing
- src/hud.c: title, score, and end-screen rendering
- images/: sprite, tile, palette, and maze assets
- music/: music data
- tools/: asset and helper generators

## Links

- Picocomputer: https://picocomputer.github.io
- llvm-mos: https://llvm-mos.org/
- RP6502 examples: https://github.com/picocomputer/examples
