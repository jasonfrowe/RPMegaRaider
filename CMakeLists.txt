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
rp6502_asset(RPMegaRaider 0x10000 images/RunningMan.bin)
rp6502_asset(RPMegaRaider 0x13400 images/HomeScreen.bin)
rp6502_asset(RPMegaRaider 0x17400 images/HomeScreen_map.bin)
rp6502_asset(RPMegaRaider help src/help.txt)

rp6502_executable(RPMegaRaider 
    DATA file 
    RESET file
)

target_sources(RPMegaRaider PRIVATE
    src/main.c
    src/input.c
    src/runningman.c
    src/maze.c
)
