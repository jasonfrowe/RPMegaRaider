cmake_minimum_required(VERSION 3.18)

add_subdirectory(tools)

set(LLVM_MOS_PLATFORM rp6502)
find_package(llvm-mos-sdk REQUIRED)

project(RPMegaRaider C CXX ASM)

# Define the option (Default is ON/Native) 
option(USE_NATIVE_OPL2 "Use the RIA native OPL2 support" ON)

if(USE_NATIVE_OPL2)
    add_definitions(-DUSE_NATIVE_OPL2)
    message(STATUS "Targeting: Native RIA OPL2")
else()
    message(STATUS "Targeting: FPGA TinyFPGA Sound Card")
endif()

add_executable(RPMegaRaider)

# Assets embedded in the ROM (auto-loaded into XRAM at boot):
rp6502_asset(RPMegaRaider 0x10000 images/RunningMan.bin)    # Player sprite  → XRAM 0x0000
rp6502_asset(RPMegaRaider 0x12400 images/EnemySprites.bin)  # Enemy sprites  → XRAM 0x2400
rp6502_asset(RPMegaRaider 0x19000 images/Shield.bin)        # Shield sprite  → XRAM 0x9000
rp6502_asset(RPMegaRaider 0x13400 images/FG_TILES.BIN)      # FG tileset     → XRAM 0x3400
rp6502_asset(RPMegaRaider 0x15400 images/BG_TILES.BIN)      # BG tileset     → XRAM 0x5400
rp6502_asset(RPMegaRaider 0x18400 images/FG_PAL.BIN)        # FG palette     → XRAM 0x8400
rp6502_asset(RPMegaRaider 0x18420 images/BG_PAL.BIN)        # BG palette     → XRAM 0x8420
rp6502_asset(RPMegaRaider help    src/help.txt)

# External USB files (too large to embed, streamed at runtime):
#   MAZE_FG.BIN, MAZE_BG.BIN          — column-major (fast horizontal scroll)
#   MAZE_FG_ROWS.BIN, MAZE_BG_ROWS.BIN — row-major   (fast vertical scroll)
# Run tools/generate_maze.py to produce all four files, then copy them to the
# root of the USB drive.

rp6502_executable(RPMegaRaider 
    DATA file 
    RESET file
)

target_sources(RPMegaRaider PRIVATE
    src/main.c
    src/input.c
    src/runningman.c
    src/stream.c
    src/enemy.c
)
