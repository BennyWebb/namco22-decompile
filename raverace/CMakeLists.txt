cmake_minimum_required(VERSION 3.16)
project(raverace C)

if(NOT CMAKE_C_COMPILER_ID STREQUAL "GNU")
  message(FATAL_ERROR "gcc required (the lifted code uses computed labels and statement expressions)")
endif()
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_EXTENSIONS ON)
option(RR_FAST_MEMORY_READS "Experiment: proven-safe WRAM/mirror reads" OFF)
option(RR_SPLIT_MEMORY_READER "Experiment: isolate WRAM reader fallback (requires RR_FAST_MEMORY_READS)" OFF)
option(RR_MEMORY_READ_COUNTERS "Instrument read counts (not performance baseline)" OFF)
option(RR_C352_VOICE_COUNTERS "Instrument C352 voice activity (not performance baseline)" OFF)
option(RR_C352_INACTIVE_FASTPATH "Experiment: skip mixing for C352 voices inactive at sample entry" OFF)
option(RR_PIPELINE_AUDIT "Diagnostic-only wall/thread-CPU pipeline attribution and draw/viewport controls" OFF)
option(RR_THREADED_PRESENTATION "Experiment: bounded immutable snapshot presentation worker" OFF)
option(RR_PARALLEL_FRAME_PREP "Experiment: prepare snapshot geometry and HUD on a separate CPU thread" OFF)
option(RR_PREP_WAKE_ON_SNAPSHOT "Experiment: wake frame preparation when a fresh snapshot arrives during the grace period" OFF)
option(RR_PARALLEL_PREP_PROFILE "Diagnostic-only CPU timing inside parallel frame preparation" OFF)
option(RR_GLES_SUBMISSION_PROFILE "Diagnostic-only sparse GLES call CPU/wall timing" OFF)
option(RR_TEXTURE_TILE_RUNS "Experiment: decode sampled texture rows in tile-bounded runs" OFF)
option(RR_TEXTURE_PREDECODE_EXPERIMENT "Experiment: prepare manifest-selected ROM pen bakes at startup" OFF)
option(RR_GLES2_MULTITEXTURE_BATCH "Experimental ordered four-texture submission (runtime default off)" OFF)
option(RR_ORDERED_SUBMISSION "Experiment: coalesce adjacent GLES commands and suppress redundant scissor state" OFF)
option(RR_GEOMETRY_PROJECTION_PROFILE "Diagnostic-only geometry phase CPU timers (environment gated)" OFF)
option(RR_GEOMETRY_REUSE_PROJECTION "Experiment: reuse unchanged near-plane projections" OFF)
option(RR_GEOMETRY_INDEX_ORDER "Experiment: GLES2 consumes sorted indices without moving full polygons" OFF)
if(RR_GLES2_MULTITEXTURE_BATCH AND NOT RR_ORDERED_SUBMISSION)
  message(FATAL_ERROR "RR_GLES2_MULTITEXTURE_BATCH requires RR_ORDERED_SUBMISSION=ON")
endif()
if(RR_PARALLEL_FRAME_PREP AND NOT RR_THREADED_PRESENTATION)
  message(FATAL_ERROR "RR_PARALLEL_FRAME_PREP requires RR_THREADED_PRESENTATION=ON")
endif()
if(RR_PARALLEL_PREP_PROFILE AND NOT RR_PARALLEL_FRAME_PREP)
  message(FATAL_ERROR "RR_PARALLEL_PREP_PROFILE requires RR_PARALLEL_FRAME_PREP=ON")
endif()
if(RR_PREP_WAKE_ON_SNAPSHOT AND NOT RR_PARALLEL_FRAME_PREP)
  message(FATAL_ERROR "RR_PREP_WAKE_ON_SNAPSHOT requires RR_PARALLEL_FRAME_PREP=ON")
endif()
if(RR_SPLIT_MEMORY_READER AND NOT RR_FAST_MEMORY_READS)
  message(FATAL_ERROR "RR_SPLIT_MEMORY_READER requires RR_FAST_MEMORY_READS=ON")
endif()
# Only rr_mem changes between variants; generated game code keeps identical flags.
if(RR_FAST_MEMORY_READS)
  set_property(SOURCE src/rr_mem.c APPEND PROPERTY COMPILE_DEFINITIONS RR_FAST_MEMORY_READS)
endif()
if(RR_SPLIT_MEMORY_READER)
  set_property(SOURCE src/rr_mem.c APPEND PROPERTY COMPILE_DEFINITIONS RR_SPLIT_MEMORY_READER)
endif()
if(RR_MEMORY_READ_COUNTERS)
  set_property(SOURCE src/rr_mem.c APPEND PROPERTY COMPILE_DEFINITIONS RR_MEMORY_READ_COUNTERS)
endif()
add_compile_options(-fno-strict-aliasing -fwrapv)

# PORTABLE (default ON): pin glibc symbol versions so a binary built here runs on
# older systems -- the Steam Deck included. See include/glibc_compat.h.
option(PORTABLE "Bind glibc symbols to baseline versions (runs on glibc >= 2.34)" ON)
if(PORTABLE AND NOT WIN32 AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
  add_compile_options(-include ${CMAKE_CURRENT_SOURCE_DIR}/include/glibc_compat.h)
endif()

include_directories(include gen)

# THE SHARED ENGINE (../engine): the same renderer, texture bake, C352 and
# sound-MCU peripheral model Prop Cycle uses (HARD RULE 3 in ../CLAUDE.md).
include(${CMAKE_CURRENT_SOURCE_DIR}/../engine/engine.cmake)
if(RR_C352_VOICE_COUNTERS)
  set_property(SOURCE ${NAMCO22_ENGINE_DIR}/c352.c APPEND PROPERTY COMPILE_DEFINITIONS C352_VOICE_COUNTERS)
endif()
if(RR_C352_INACTIVE_FASTPATH)
  set_property(SOURCE ${NAMCO22_ENGINE_DIR}/c352.c APPEND PROPERTY COMPILE_DEFINITIONS C352_INACTIVE_FASTPATH)
endif()

# The sound program, translated to C from the ROM files (tools/gen/snd_translate.py).
# Input: tools/gen/snd.cov -- the (address, M/X, length) of every instruction the
# test oracle executed (addresses only, no ROM data; committed). Regenerate it with
#   RR_SNDCOV=f ./build/rr_sndoracle extracted ... (merge runs, sort -u)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_custom_command(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/gen/snd_driver.c
  COMMAND ${Python3_EXECUTABLE} tools/gen/snd_translate.py tools/gen/snd.cov
  DEPENDS tools/gen/snd_translate.py tools/gen/m7700_decode.py tools/gen/snd_rom.py ../tools/gen/snd_spin.inc tools/gen/snd.cov
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMENT "Translating the C74 sound program to C")

# The MASTER DSP program (the C71 BIOS + the block the 68K uploads), translated
# to C from the ROM files by the shared tool ../tools/gen/c25_translate.py.
# tools/gen/c25.cov: the addresses the interpreter oracle executed (addresses
# only, no ROM data; committed). The game has no C25 fetch/decode loop.
add_custom_command(OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/gen/rr_c25.c
  COMMAND ${Python3_EXECUTABLE} ../tools/gen/c25_translate.py --game rr --roms extracted
          --cov tools/gen/c25.cov --out gen/rr_c25.c --func rr_c25_exec
          # the game's program (FUN_000318BE) and the test-mode ones (FUN_00006DA2's calls)
          --block 31A68 --block 6DE6 --block 6E46 --block 6F3E --block 707E --block 72FE --block 75EE --block 764C
  DEPENDS ../tools/gen/c25_translate.py tools/gen/c25.cov ../engine/c25/c71_bios.inc
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMENT "Translating the master DSP program to C")
set_source_files_properties(gen/rr_c25.c PROPERTIES COMPILE_OPTIONS "-O2;-w")

# gen/rr_lifted.c is GENERATED by tools/gen/lift.py -- 8 MB of goto-heavy C;
# -O1 keeps the build to a minute and is plenty fast for a 25 MHz 68020.
set_source_files_properties(gen/rr_lifted.c PROPERTIES COMPILE_OPTIONS "-O1;-w")

# Readable-C replacements (Phase B): every file in src/rd/ registers itself.
file(GLOB RD_SRC CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/rd/*.c)

option(RR_HEADLESS_ONLY "Build only the deterministic scheduler test (no SDL/OpenGL)" OFF)
option(RR_GLES2 "Build the GLES2 scene viewer (no desktop GL/menu)" OFF)
option(RR_GPU_HUD "Prototype: compose packed System 22 character/tile state on GPU (runtime opt-in)" OFF)
if(RR_GLES2 AND RR_HEADLESS_ONLY)
  message(FATAL_ERROR "Choose RR_GLES2 or RR_HEADLESS_ONLY in separate build directories")
endif()
if(RR_HEADLESS_ONLY OR RR_GLES2)
  if(RR_GLES2)
    foreach(flag RR_GEOMETRY_PROJECTION_PROFILE RR_GEOMETRY_REUSE_PROJECTION)
      if(${flag})
        set_property(SOURCE ${NAMCO22_ENGINE_DIR}/geo_hw.c APPEND PROPERTY COMPILE_DEFINITIONS ${flag})
      endif()
    endforeach()
    if(RR_GEOMETRY_INDEX_ORDER)
      set_property(SOURCE src/rr_gl.c ${NAMCO22_ENGINE_DIR}/quad_gles2.c
        APPEND PROPERTY COMPILE_DEFINITIONS RR_GEOMETRY_INDEX_ORDER)
    endif()
    if(RR_GLES_SUBMISSION_PROFILE)
      set_property(SOURCE ${NAMCO22_ENGINE_DIR}/quad_gles2.c ${NAMCO22_ENGINE_DIR}/tex_bake.c APPEND PROPERTY COMPILE_DEFINITIONS RR_GLES_SUBMISSION_PROFILE)
    endif()
    if(RR_GLES2_MULTITEXTURE_BATCH)
      set_property(SOURCE ${NAMCO22_ENGINE_DIR}/quad_gles2.c APPEND PROPERTY COMPILE_DEFINITIONS RR_GLES2_MULTITEXTURE_BATCH)
    endif()
    if(RR_TEXTURE_TILE_RUNS)
      set_property(SOURCE ${NAMCO22_ENGINE_DIR}/tex_bake.c APPEND PROPERTY COMPILE_DEFINITIONS RR_TEXTURE_TILE_RUNS)
    endif()
    if(RR_TEXTURE_PREDECODE_EXPERIMENT)
      set_property(SOURCE src/rr_gl.c ${NAMCO22_ENGINE_DIR}/tex_bake.c APPEND PROPERTY COMPILE_DEFINITIONS RR_TEXTURE_PREDECODE_EXPERIMENT)
    endif()
    if(RR_ORDERED_SUBMISSION)
      set_property(SOURCE ${NAMCO22_ENGINE_DIR}/quad_gles2.c APPEND PROPERTY COMPILE_DEFINITIONS RR_ORDERED_SUBMISSION)
    endif()
    set(RR_TEST_TARGET rr_gles2)
  else()
    set(RR_TEST_TARGET rr_headless)
  endif()
  add_executable(${RR_TEST_TARGET} src/rr_main.c src/rr_input_stream.c src/rr_mem.c src/rr_hw.c src/rr_game.c
    src/rr_env.c src/rr_link.c src/rr_dsp.c src/rr_scene.c src/rr_audio.c
    src/snd/snd_board.c gen/snd_driver.c gen/rr_lifted.c gen/rr_c25.c
    ${RD_SRC} ${NAMCO22_ENGINE_LIFT_SRC} ${NAMCO22_ENGINE_C25_SRC}
    ${NAMCO22_ENGINE_DIR}/c352.c ${NAMCO22_ENGINE_DIR}/snd/m377_periph.c)
  target_compile_definitions(${RR_TEST_TARGET} PRIVATE RR_HEADLESS)
  target_include_directories(${RR_TEST_TARGET} PRIVATE src/snd)
  target_compile_options(${RR_TEST_TARGET} PRIVATE -Wall -Wno-unused-function)
  set_source_files_properties(gen/snd_driver.c PROPERTIES COMPILE_OPTIONS "-O1;-w")
  target_link_libraries(${RR_TEST_TARGET} PRIVATE m)
  if(RR_GLES2)
    target_sources(${RR_TEST_TARGET} PRIVATE src/rr_sim_profile.c)
    target_sources(${RR_TEST_TARGET} PRIVATE ${NAMCO22_ENGINE_DIR}/eng_audit.c)
    if(RR_PIPELINE_AUDIT)
      set_property(SOURCE src/rr_gles2.c src/rr_sim_profile.c
        ${NAMCO22_ENGINE_DIR}/eng_audit.c ${NAMCO22_ENGINE_DIR}/quad_gles2.c
        ${NAMCO22_ENGINE_DIR}/tex_bake.c APPEND PROPERTY COMPILE_DEFINITIONS RR_PIPELINE_AUDIT)
    endif()
    target_compile_options(rr_gles2 PRIVATE -Wno-unused-variable)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(RR_SDL REQUIRED IMPORTED_TARGET sdl2)
    find_path(RR_GLES_INCLUDE GLES2/gl2.h)
    find_library(RR_GLES_LIBRARY NAMES GLESv2)
    find_library(RR_EGL_LIBRARY NAMES EGL)
    find_library(RR_MALI_LIBRARY NAMES mali)
    if(NOT RR_GLES_INCLUDE OR NOT RR_GLES_LIBRARY OR NOT RR_EGL_LIBRARY)
      message(FATAL_ERROR "GLES2/EGL development files are required in the target sysroot")
    endif()
    set(RR_GLES2_TEXTURE_BUDGET_KB 65536 CACHE STRING "Default GLES2 texture allocation budget in KiB")
    set(RR_GLES2_TEXTURE_MAX_BUDGET_KB 131072 CACHE STRING "Maximum runtime GLES2 texture budget in KiB")
    if(NOT RR_GLES2_TEXTURE_BUDGET_KB MATCHES "^[1-9][0-9]*$" OR RR_GLES2_TEXTURE_BUDGET_KB GREATER 131072)
      message(FATAL_ERROR "RR_GLES2_TEXTURE_BUDGET_KB must be 1..131072")
    endif()
    if(NOT RR_GLES2_TEXTURE_MAX_BUDGET_KB MATCHES "^[1-9][0-9]*$" OR
       RR_GLES2_TEXTURE_MAX_BUDGET_KB GREATER 131072 OR
       RR_GLES2_TEXTURE_MAX_BUDGET_KB LESS RR_GLES2_TEXTURE_BUDGET_KB)
      message(FATAL_ERROR "RR_GLES2_TEXTURE_MAX_BUDGET_KB must be >= default and <= 131072")
    endif()
    if(RR_THREADED_PRESENTATION)
      if(RR_PIPELINE_AUDIT)
        message(FATAL_ERROR "Threaded presentation uses its own two-thread profiler; RR_PIPELINE_AUDIT must be OFF")
      endif()
      find_package(Threads REQUIRED)
      target_sources(rr_gles2 PRIVATE src/rr_presentation.c)
      target_compile_definitions(rr_gles2 PRIVATE RR_THREADED_PRESENTATION)
      if(RR_PARALLEL_FRAME_PREP)
        if(NOT RR_GEOMETRY_INDEX_ORDER)
          message(FATAL_ERROR "RR_PARALLEL_FRAME_PREP requires RR_GEOMETRY_INDEX_ORDER=ON")
        endif()
        target_compile_definitions(rr_gles2 PRIVATE RR_PARALLEL_FRAME_PREP)
        if(RR_PREP_WAKE_ON_SNAPSHOT)
          target_compile_definitions(rr_gles2 PRIVATE RR_PREP_WAKE_ON_SNAPSHOT)
        endif()
        if(RR_PARALLEL_PREP_PROFILE)
          target_compile_definitions(rr_gles2 PRIVATE RR_PARALLEL_PREP_PROFILE)
        endif()
        set_property(SOURCE ${NAMCO22_ENGINE_DIR}/quad_gles2.c
          APPEND PROPERTY COMPILE_DEFINITIONS RR_PARALLEL_FRAME_PREP)
      endif()
      target_link_libraries(rr_gles2 PRIVATE Threads::Threads)
    endif()
    target_sources(rr_gles2 PRIVATE src/rr_gles2.c src/rr_gl.c src/rr_video_layers.c src/rr_overlay.c
      ${NAMCO22_ENGINE_DIR}/quad_gles2.c ${NAMCO22_ENGINE_DIR}/tex_bake.c ${NAMCO22_ENGINE_DIR}/eng.c
      ${NAMCO22_ENGINE_DIR}/geo_hw.c ${NAMCO22_ENGINE_DIR}/slave_list.c)
    if(RR_GPU_HUD)
      target_compile_definitions(rr_gles2 PRIVATE RR_GPU_HUD)
    endif()
    target_compile_definitions(rr_gles2 PRIVATE RR_GLES2
      RR_GLES2_TEXTURE_BUDGET_KB=${RR_GLES2_TEXTURE_BUDGET_KB}
      RR_GLES2_TEXTURE_MAX_BUDGET_KB=${RR_GLES2_TEXTURE_MAX_BUDGET_KB})
    target_include_directories(rr_gles2 PRIVATE ${RR_GLES_INCLUDE})
    target_link_libraries(rr_gles2 PRIVATE PkgConfig::RR_SDL ${RR_GLES_LIBRARY} ${RR_EGL_LIBRARY})
    if(RR_MALI_LIBRARY)
      # H700 SDK EGL/GLES wrapper DSOs forward symbols to libmali.
      target_link_libraries(rr_gles2 PRIVATE ${RR_MALI_LIBRARY})
    endif()
  endif()
  return()
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
if(WIN32)
  set(ZLIB_USE_STATIC_LIBS ON)   # MSYS2's zlib comes as a DLL too: link the static one, or the .exe needs zlib1.dll (CMake 3.24+)
endif()
find_package(ZLIB REQUIRED)
set(OpenGL_GL_PREFERENCE GLVND)
find_package(OpenGL REQUIRED)
# Windows (MinGW-w64, ./build-windows.sh at the top of the tree): the few POSIX calls
# mapped in include/win_compat.h, no console window, and everything linked in --
# SDL2 statically too -- so RaveRacer.exe needs no DLL beside it. (Every DLL it
# imports ships with Windows. OpenGL is NOT linked: the shared engine's
# engine/gl_dyn.c resolves it at run time, so the bundled Mesa can stand in
# when Windows has no real driver -- exactly as Prop Cycle.)
if(WIN32)
  add_compile_options(-include ${CMAKE_CURRENT_SOURCE_DIR}/include/win_compat.h)
  add_link_options(-mwindows -static-libgcc -static)
  set(SDL2_LIBRARIES ${SDL2_STATIC_LDFLAGS})
  set(RR_GL_LIB "")
  set(RR_GL_SRC ${NAMCO22_ENGINE_DIR}/gl_dyn.c)
  set(RR_NET_LIB ws2_32)   # rr_net.c: Winsock2
else()
  set(RR_GL_LIB OpenGL::GL)
  set(RR_GL_SRC "")
  set(RR_NET_LIB "")
endif()

add_executable(rr src/rr_win.c src/rr_romzip.c ${NAMCO22_ENGINE_DIR}/romzip.c src/rr_main.c ${NAMCO22_ENGINE_LIFT_SRC} src/rr_mem.c src/rr_hw.c src/rr_game.c src/rr_env.c src/rr_link.c src/rr_net.c ${NAMCO22_ENGINE_NET_SRC} src/rr_dsp.c ${NAMCO22_ENGINE_C25_SRC} gen/rr_c25.c src/rr_scene.c src/rr_gl.c ${NAMCO22_ENGINE_GL_SRC} ${RR_GL_SRC} src/rr_host.c src/rr_ui.c src/rr_input.c ${NAMCO22_ENGINE_FFB_SRC} src/rr_audio.c ${NAMCO22_ENGINE_SND_SRC} src/snd/snd_board.c gen/snd_driver.c gen/rr_lifted.c ${RD_SRC})
target_compile_options(rr PRIVATE -Wall)
target_include_directories(rr PRIVATE src/snd)
# gen/snd_driver.c is GENERATED by tools/gen/snd_translate.py (the C74 sound program
# translated to C from the ROM files); like gen/rr_lifted.c it is big and -O1 is plenty.
set_source_files_properties(gen/snd_driver.c PROPERTIES COMPILE_OPTIONS "-O1;-w")
target_include_directories(rr PRIVATE ${SDL2_INCLUDE_DIRS})
target_link_libraries(rr ${SDL2_LIBRARIES} ${RR_GL_LIB} ZLIB::ZLIB ${RR_NET_LIB} m)

# Development tools (the instruction-trace build and the sound test oracle):
# only when their sources are present -- the public tree has just the game.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tools/sndoracle/sound_oracle.c)
# instruction-trace build: every instruction logs PC + D0-D7/A0-A7
add_executable(rr_trace ${NAMCO22_C25_ORACLE_SRC} src/rr_romzip.c ${NAMCO22_ENGINE_DIR}/romzip.c src/rr_main.c ${NAMCO22_ENGINE_LIFT_SRC} src/rr_mem.c src/rr_hw.c src/rr_game.c src/rr_env.c src/rr_link.c src/rr_net.c ${NAMCO22_ENGINE_NET_SRC} src/rr_dsp.c ${NAMCO22_ENGINE_C25_SRC} gen/rr_c25.c src/rr_video.c src/rr_scene.c src/rr_gl.c ${NAMCO22_ENGINE_GL_SRC} ${RR_GL_SRC} src/rr_host.c src/rr_ui.c src/rr_input.c ${NAMCO22_ENGINE_FFB_SRC} src/rr_audio.c ${NAMCO22_ENGINE_SND_SRC} src/snd/snd_board.c gen/snd_driver.c gen/rr_lifted.c ${RD_SRC})
target_include_directories(rr_trace PRIVATE ${SDL2_INCLUDE_DIRS} src/snd)
target_link_libraries(rr_trace ${SDL2_LIBRARIES} OpenGL::GL ZLIB::ZLIB ${RR_NET_LIB} m)
target_compile_definitions(rr_trace PRIVATE RR_TRACE RR_ORACLE C25_DEV_HOOKS)

# THE RENDERER ORACLE: the game with the software MAME port of namcos22_v.cpp
# (src/rr_video.c) built in beside the engine -- --render-dump without --gl is
# pixel-exact against MAME's own snapshots, and is what the engine's OpenGL
# picture is measured against. Never shipped (HARD RULE 2).
add_executable(rr_oracle ${NAMCO22_C25_ORACLE_SRC} src/rr_romzip.c ${NAMCO22_ENGINE_DIR}/romzip.c src/rr_main.c ${NAMCO22_ENGINE_LIFT_SRC} src/rr_mem.c src/rr_hw.c src/rr_game.c src/rr_env.c src/rr_link.c src/rr_net.c ${NAMCO22_ENGINE_NET_SRC} src/rr_dsp.c ${NAMCO22_ENGINE_C25_SRC} gen/rr_c25.c src/rr_video.c src/rr_scene.c src/rr_gl.c ${NAMCO22_ENGINE_GL_SRC} ${RR_GL_SRC} src/rr_host.c src/rr_ui.c src/rr_input.c ${NAMCO22_ENGINE_FFB_SRC} src/rr_audio.c ${NAMCO22_ENGINE_SND_SRC} src/snd/snd_board.c gen/snd_driver.c gen/rr_lifted.c ${RD_SRC})
target_include_directories(rr_oracle PRIVATE ${SDL2_INCLUDE_DIRS} src/snd)
target_link_libraries(rr_oracle ${SDL2_LIBRARIES} OpenGL::GL ZLIB::ZLIB ${RR_NET_LIB} m)
target_compile_definitions(rr_oracle PRIVATE RR_ORACLE C25_DEV_HOOKS)

# TEST ORACLE: the same engine with the ORIGINAL C74 sound program fetched and
# decoded from memory in place of the translated driver. Never shipped -- the game
# (rr) links gen/snd_driver.c and contains no fetch/decode loop. See tools/sndoracle/.
add_executable(rr_sndoracle ${NAMCO22_C25_ORACLE_SRC} src/rr_romzip.c ${NAMCO22_ENGINE_DIR}/romzip.c src/rr_main.c ${NAMCO22_ENGINE_LIFT_SRC} src/rr_mem.c src/rr_hw.c src/rr_game.c src/rr_env.c src/rr_link.c src/rr_net.c ${NAMCO22_ENGINE_NET_SRC} src/rr_dsp.c ${NAMCO22_ENGINE_C25_SRC} gen/rr_c25.c src/rr_video.c src/rr_scene.c src/rr_gl.c ${NAMCO22_ENGINE_GL_SRC} ${RR_GL_SRC} src/rr_host.c src/rr_ui.c src/rr_input.c ${NAMCO22_ENGINE_FFB_SRC} src/rr_audio.c ${NAMCO22_ENGINE_SND_SRC} src/snd/snd_board.c tools/sndoracle/sound_oracle.c tools/sndoracle/m37710.c gen/rr_lifted.c ${RD_SRC})
target_include_directories(rr_sndoracle PRIVATE ${SDL2_INCLUDE_DIRS} src/snd tools/sndoracle)
target_compile_definitions(rr_sndoracle PRIVATE RR_ORACLE C25_DEV_HOOKS)
target_link_libraries(rr_sndoracle ${SDL2_LIBRARIES} OpenGL::GL ZLIB::ZLIB ${RR_NET_LIB} m)
endif()
