file(READ "${CMAKE_CURRENT_SOURCE_DIR}/../platform/knulli-h700/device-profile.json" DEVICE_PROFILE)
string(JSON GLES3_QUEUE_DEPTH GET "${DEVICE_PROFILE}" queue_depth)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)
find_library(GLES_LIBRARY GLESv2 REQUIRED)
find_library(EGL_LIBRARY EGL REQUIRED)
find_library(CRYPTO_LIBRARY NAMES crypto libcrypto.so.3 REQUIRED)
find_library(MALI_LIBRARY mali)
set(RR_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(GENERATED ${CMAKE_BINARY_DIR}/generated)
configure_file(${RR_ROOT}/gen/rr_lifted.c ${GENERATED}/rr_lifted.c COPYONLY)
set_source_files_properties(${GENERATED}/rr_lifted.c PROPERTIES COMPILE_OPTIONS "-O2;-w")
set_source_files_properties(${GENERATED}/snd_driver.c PROPERTIES COMPILE_OPTIONS "-O1;-w")
set_source_files_properties(${GENERATED}/rr_c25.c PROPERTIES COMPILE_OPTIONS "-O2;-w")
set_source_files_properties(${GENERATED}/rr_c25.c ${GENERATED}/snd_driver.c PROPERTIES GENERATED TRUE)
set(COMMON src/main.c ${RR_ROOT}/src/rr_mem.c src/scene.c src/presentation.c src/hud.c ${RR_ROOT}/src/rr_audio.c src/unsupported.c
 ${RR_ROOT}/src/rr_input_stream.c ${RR_ROOT}/src/rr_hw.c ${RR_ROOT}/src/rr_game.c
 ${RR_ROOT}/src/rr_env.c ${RR_ROOT}/src/rr_link.c ${RR_ROOT}/src/rr_dsp.c
 ${RR_ROOT}/src/rr_overlay.c ${RR_ROOT}/src/rr_sim_profile.c ${RR_ROOT}/src/snd/snd_board.c
 src/lifted_only.c
 ${NAMCO22_ENGINE_LIFT_SRC} ${NAMCO22_ENGINE_C25_SRC}
 ${NAMCO22_ENGINE_DIR}/eng.c ${NAMCO22_ENGINE_DIR}/c352.c ${NAMCO22_ENGINE_DIR}/snd/m377_periph.c)
add_library(gles3_generated OBJECT ${GENERATED}/rr_lifted.c ${GENERATED}/rr_c25.c)
add_dependencies(gles3_generated rr_generated)
target_compile_definitions(gles3_generated PRIVATE RR_HEADLESS RR_GLES3)
target_include_directories(gles3_generated PRIVATE ${RR_ROOT}/include ${RR_ROOT}/gen ${GENERATED} ${NAMCO22_ENGINE_DIR} ${NAMCO22_ENGINE_DIR}/c25)
foreach(core gles3_core gles3_validation_core gles3_setup_core)
 add_library(${core} OBJECT ${COMMON})
 add_dependencies(${core} rr_generated)
 target_compile_definitions(${core} PUBLIC _GNU_SOURCE RR_HEADLESS RR_GLES3 RR_GPU_HUD RR_GPU_HUD_DEFAULT
  RR_THREADED_PRESENTATION RR_PARALLEL_FRAME_PREP RR_FAST_MEMORY_READS RR_SPLIT_MEMORY_READER C352_INACTIVE_FASTPATH RR_AUDIO_RESAMPLER_TELEMETRY GLES3_QUEUE_DEPTH=${GLES3_QUEUE_DEPTH})
 target_include_directories(${core} PUBLIC src ${RR_ROOT}/src ${RR_ROOT}/src/snd ${RR_ROOT}/include ${RR_ROOT}/gen ${GENERATED} ${NAMCO22_ENGINE_DIR} ${NAMCO22_ENGINE_DIR}/snd ${NAMCO22_ENGINE_DIR}/c25)
 target_compile_options(${core} PRIVATE -O2 -fno-strict-aliasing -fwrapv -Wall -Wextra)
 target_link_libraries(${core} PUBLIC PkgConfig::SDL2 Threads::Threads ZLIB::ZLIB ${GLES_LIBRARY} ${EGL_LIBRARY} ${CRYPTO_LIBRARY} m)
 if(MALI_LIBRARY)
  target_link_libraries(${core} PUBLIC ${MALI_LIBRARY})
 endif()
endforeach()
target_compile_definitions(gles3_validation_core PUBLIC GLES3_VALIDATION)
target_compile_definitions(gles3_setup_core PUBLIC GLES3_EEPROM_SETUP)
foreach(target rr_gles3 rr_gles3_validation rr_gles3_sound_oracle rr_gles3_setup)
 set(driver ${GENERATED}/snd_driver.c)
 if(target STREQUAL rr_gles3_sound_oracle)
  set(driver ${RR_ROOT}/tools/sndoracle/sound_oracle.c ${RR_ROOT}/tools/sndoracle/m37710.c)
 endif()
 set(io src/async_play.c)
 if(target STREQUAL rr_gles3_validation)
  set(io tests/async_io.c tests/boot_oracle.c)
 endif()
 add_executable(${target} src/live.c src/gles.c src/layers.c ${io} ${driver})
 if(target STREQUAL rr_gles3_validation)
  target_link_libraries(${target} PRIVATE gles3_validation_core gles3_generated)
 elseif(target STREQUAL rr_gles3_setup)
  target_link_libraries(${target} PRIVATE gles3_setup_core gles3_generated)
 else()
  target_link_libraries(${target} PRIVATE gles3_core gles3_generated)
 endif()
 target_compile_options(${target} PRIVATE -O2 -fno-strict-aliasing -fwrapv -Wall -Wextra)
 target_link_options(${target} PRIVATE -Wl,--wrap=fopen -Wl,--wrap=fopen64 -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${target}.map)
 if(target STREQUAL rr_gles3_validation)
  target_compile_definitions(${target} PRIVATE GLES3_VALIDATION)
 endif()
endforeach()
target_sources(rr_gles3 PRIVATE src/operator_settings.c src/title_input.c src/title_state.c src/title_draw.c src/title.c)
target_compile_definitions(rr_gles3 PRIVATE RR_TITLE_OPTIONS)

# Test-only EEPROM capture: the same lifted setup core, without changing its
# seed/reset rules or adding an export hook to the product.
if(NOT CMAKE_CROSSCOMPILING)
 add_library(title_open_audit SHARED tests/title_open_audit.c)
 target_link_libraries(title_open_audit PRIVATE dl)
 set_target_properties(title_open_audit PROPERTIES PREFIX "")
 add_executable(test_title_runtime src/live.c src/gles.c src/layers.c src/async_play.c
  ${GENERATED}/snd_driver.c src/operator_settings.c src/title_input.c src/title_state.c src/title_draw.c src/title.c
  tests/title_runtime_driver.c)
 target_compile_definitions(test_title_runtime PRIVATE RR_TITLE_OPTIONS)
 target_link_libraries(test_title_runtime PRIVATE gles3_core gles3_generated dl)
 target_link_options(test_title_runtime PRIVATE -Wl,--wrap=fopen -Wl,--wrap=fopen64)
 target_compile_options(test_title_runtime PRIVATE -O2 -Wall -Wextra)
 add_executable(test_title tests/test_title.c src/title_input.c src/title_state.c src/title_draw.c src/operator_settings.c)
 target_include_directories(test_title PRIVATE src ${RR_ROOT}/include)
 target_link_libraries(test_title PRIVATE PkgConfig::SDL2 ${GLES_LIBRARY})
 target_compile_options(test_title PRIVATE -O2 -UNDEBUG -Wall -Wextra)
 add_executable(rr_operator_oracle src/live.c src/gles.c src/layers.c src/async_play.c
  ${GENERATED}/snd_driver.c tests/operator_oracle_export.c)
 target_link_libraries(rr_operator_oracle PRIVATE gles3_setup_core gles3_generated)
 target_compile_options(rr_operator_oracle PRIVATE -O2 -fno-strict-aliasing -fwrapv -Wall -Wextra)
 target_link_options(rr_operator_oracle PRIVATE -Wl,--wrap=fopen -Wl,--wrap=fopen64)

 add_executable(test_operator_settings tests/test_operator_settings.c tests/test_operator_io.c src/operator_settings.c)
 target_include_directories(test_operator_settings PRIVATE src)
 target_compile_options(test_operator_settings PRIVATE -O2 -UNDEBUG -Wall -Wextra)
 target_link_options(test_operator_settings PRIVATE -Wl,--wrap=fsync -Wl,--wrap=write
  -Wl,--wrap=read -Wl,--wrap=__read_chk -Wl,--wrap=renameat)
endif()

add_executable(rr_asset_builder assets/asset_builder.c)
add_library(gles3_session_io SHARED ../platform/knulli-h700/session_io.c)
set_target_properties(gles3_session_io PROPERTIES PREFIX "" OUTPUT_NAME "session-io")
add_library(gles3_smoke_chord SHARED ../platform/knulli-h700/smoke_chord.c)
target_link_libraries(gles3_smoke_chord PRIVATE PkgConfig::SDL2 dl)
set_target_properties(gles3_smoke_chord PROPERTIES PREFIX "" OUTPUT_NAME "smoke-chord")

add_custom_target(rr_asset_kit
 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/assets/compile.py ${RR_ROM_DIR} ${CMAKE_BINARY_DIR}/rom-assets
 COMMENT "Compile ROM-only layout and metadata into the build directory")

foreach(test projection cull cache staging)
 add_executable(test_${test} tests/test_${test}.c)
 target_include_directories(test_${test} PRIVATE src)
 target_compile_options(test_${test} PRIVATE -O2 -UNDEBUG -fwrapv -fno-strict-aliasing -Wall -Wextra)
 target_link_libraries(test_${test} PRIVATE m)
 add_test(NAME ${test} COMMAND test_${test})
endforeach()

foreach(target rr_asset_builder gles3_session_io gles3_smoke_chord)
 target_compile_options(${target} PRIVATE -Wall -Wextra)
endforeach()
add_custom_target(rr_validation_asset_kit
 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/assets/compile.py ${RR_ROM_DIR} ${CMAKE_BINARY_DIR}/validation-assets --validation
 COMMENT "Compile ROM-only validation assets into the build directory")

add_test(NAME recording_inputs
 COMMAND ${Python3_EXECUTABLE} -O ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_recording_inputs.py)

add_executable(rr_message ../platform/knulli-h700/message.c)
target_link_libraries(rr_message PRIVATE PkgConfig::SDL2)
target_include_directories(rr_message PRIVATE ${RR_ROOT}/include)
target_compile_options(rr_message PRIVATE -Wall -Wextra)

add_library(rr_asset_codec SHARED assets/stream_codec.c)
set_target_properties(rr_asset_codec PROPERTIES PREFIX "" OUTPUT_NAME "rr_asset_codec")
target_link_libraries(rr_asset_codec PRIVATE ${CRYPTO_LIBRARY})
target_compile_options(rr_asset_codec PRIVATE -O2 -Wall -Wextra)
add_test(NAME session_child_guard
 COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_child_guard.py)
