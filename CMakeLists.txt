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
rp6502_asset(RPMegaRaider help           src/help.txt)
rp6502_asset(RPMegaRaider MAZE_FG.BIN    images/MAZE_FG.BIN)
rp6502_asset(RPMegaRaider MAZE_BG.BIN    images/MAZE_BG.BIN)
rp6502_asset(RPMegaRaider MAZE_FG_ROWS.BIN images/MAZE_FG_ROWS.BIN)
rp6502_asset(RPMegaRaider MAZE_BG_ROWS.BIN images/MAZE_BG_ROWS.BIN)
rp6502_asset(RPMegaRaider SPAWNS.BIN     images/SPAWNS.BIN)
rp6502_asset(RPMegaRaider COSMIC1.BIN    music/COSMIC1.BIN)
rp6502_asset(RPMegaRaider RAIDER1.BIN    music/RAIDER1.BIN)

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
    src/hud.c
    src/opl.c
    src/instruments.c
    src/sound.c
)
