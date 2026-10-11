cmake_minimum_required(VERSION 3.18)
project(knulli_gles_test LANGUAGES CXX)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
# Vendor sysroots sometimes have GLES headers/libraries but no glesv2.pc.
find_path(GLES2_INCLUDE_DIR GLES2/gl2.h REQUIRED)
find_library(GLES2_LIBRARY NAMES GLESv2 REQUIRED)
find_library(MALI_LIBRARY NAMES mali REQUIRED)

add_executable(knulli-gles-test main.cpp)
target_compile_features(knulli-gles-test PRIVATE cxx_std_17)
set_target_properties(knulli-gles-test PROPERTIES CXX_EXTENSIONS OFF)
target_compile_options(knulli-gles-test PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(knulli-gles-test PRIVATE ${GLES2_INCLUDE_DIR})
target_link_libraries(knulli-gles-test PRIVATE
    PkgConfig::SDL2
    ${GLES2_LIBRARY}
    ${MALI_LIBRARY}
)
install(TARGETS knulli-gles-test RUNTIME DESTINATION bin)
