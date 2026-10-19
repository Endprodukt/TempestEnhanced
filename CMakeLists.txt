cmake_minimum_required(VERSION 3.21)

project(tempest_enhanced VERSION 0.1.23 LANGUAGES C CXX)

# Builds: see BUILD.md.

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(TP_BUILD_GAME "Build the game itself (needs the VectorPie SDK and SDL2)" ON)

# --- third party ------------------------------------------------------------

# miniz (MIT): reads the MAME ROM zip.
add_library(miniz STATIC third_party/miniz/miniz.c)
target_include_directories(miniz PUBLIC third_party/miniz)
target_compile_options(miniz PRIVATE -w)

# --- the board: no SDK, no SDL, testable on its own ---------------------------

add_library(tpcore STATIC
    src/core/machine.c
    src/core/emu.c
    src/core/earom.c
    src/core/hiscore.c
    src/core/pokeyio.c
    src/cpu/m6502.c
    src/mbox/mathbox.c
    src/avg/avg.c
    src/avg/avgsm.c
    src/rom/romset.c
    src/capture/capture.c
    src/sound/pokey.c
    src/sound/sound.c
    src/render/svg.c
    src/render/layout.c
    src/render/fireworks.c
    src/render/nextlevel.c
    src/render/enhance.c
    src/render/logo.c
    src/render/config.c
    src/render/smooth.c
    src/render/frames.c
)
target_include_directories(tpcore PUBLIC src)
target_link_libraries(tpcore PUBLIC miniz)
target_compile_options(tpcore PRIVATE -Wall -Wextra)
if(UNIX)
    target_link_libraries(tpcore PUBLIC m)
endif()

# --- tools ---------------------------------------------------------------------

# Runs the game without a screen and writes its picture (SVG).
add_executable(tpshot tools/tpshot/main.c)
target_link_libraries(tpshot PRIVATE tpcore)
target_compile_options(tpshot PRIVATE -Wall -Wextra)

# Checks the board frame by frame against a MAME recording.
add_executable(tpcompare tools/tpcompare/main.c)
target_link_libraries(tpcompare PRIVATE tpcore)
target_compile_options(tpcompare PRIVATE -Wall -Wextra)

# Runs the board on its own from power-on beside a MAME recording.
add_executable(tplockstep tools/tplockstep/main.c)
target_link_libraries(tplockstep PRIVATE tpcore)
target_compile_options(tplockstep PRIVATE -Wall -Wextra)

# --- the game: on the VectorPie SDK (a window on a PC, the vector board on the cabinet) ---

if(TP_BUILD_GAME)
    set(TP_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/VectorPie/sdk"
        CACHE PATH "VectorPie SDK source directory")
    if(NOT EXISTS "${TP_SDK_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "VectorPie SDK not found in ${TP_SDK_DIR}: see BUILD.md "
                            "(or configure with -DTP_BUILD_GAME=OFF for the tools only)")
    endif()
    if(TP_PI_BUILD)
        # cmake/toolchain-pi-clang.cmake (the Pi build from Windows, without
        # WSL): the cabinet's own SDL2, by its file.
        add_library(SDL2::SDL2 SHARED IMPORTED GLOBAL)
        set_target_properties(SDL2::SDL2 PROPERTIES
            IMPORTED_LOCATION "${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu/libSDL2-2.0.so.0"
            IMPORTED_SONAME "libSDL2-2.0.so.0"
            INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_SYSROOT}/usr/include/SDL2")
        set(THREADS_PREFER_PTHREAD_FLAG ON)
    endif()
    set(VP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    set(VP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    add_subdirectory("${TP_SDK_DIR}" vectorpie EXCLUDE_FROM_ALL)

    add_library(tpout STATIC
        src/render/draw.c
        src/render/title.c
        src/platform/app.c
        src/platform/audio.c
        src/platform/controls.c
        src/platform/menu.c
        src/platform/vpbind.c
    )
    target_link_libraries(tpout PUBLIC tpcore vectorpie)
    target_compile_options(tpout PRIVATE -Wall -Wextra)

    add_executable(tempest src/main.c)
    target_link_libraries(tempest PRIVATE tpout)
    vp_program(tempest)

    # package files next to the program (vp_asset_path looks there)
    configure_file(package/tempest.ini ${CMAKE_CURRENT_BINARY_DIR}/tempest.ini COPYONLY)
    configure_file(package/marquee.png ${CMAKE_CURRENT_BINARY_DIR}/marquee.png COPYONLY)
    configure_file(package/game.ini ${CMAKE_CURRENT_BINARY_DIR}/game.ini COPYONLY)
endif()

# The board's sound against MAME's (WAV files of the same run).
add_executable(tpsoundcmp tools/tpsoundcmp/main.c)
target_compile_options(tpsoundcmp PRIVATE -Wall -Wextra)
if(UNIX)
    target_link_libraries(tpsoundcmp PRIVATE m)
endif()
