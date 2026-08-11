cmake_minimum_required(VERSION 3.16)

project(digs_android LANGUAGES C CXX)

get_filename_component(VOX_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../../../../../../" ABSOLUTE)

if (NOT EXISTS "${VOX_ROOT}/third_party/SDL/CMakeLists.txt")
    message(FATAL_ERROR
        "The pinned SDL2 submodule is required. Run: git submodule update --init --recursive")
endif()

set(SDL_SHARED ON CACHE BOOL "Build SDL2 as an Android shared library" FORCE)
set(SDL_STATIC OFF CACHE BOOL "Do not build the unused SDL2 static library" FORCE)
set(SDL_TEST OFF CACHE BOOL "Do not build SDL2's test programs" FORCE)
set(SDL_DISABLE_INSTALL ON CACHE BOOL "Do not install SDL2 while building the APK" FORCE)
add_subdirectory("${VOX_ROOT}/third_party/SDL" "${CMAKE_BINARY_DIR}/sdl2")

set(VOX_BUILD_TESTS OFF CACHE BOOL "Android APK builds do not include CTest" FORCE)
set(VOX_BUILD_SDL2_DEMO ON CACHE BOOL "Build the SDL2 DIGS host" FORCE)
set(VOX_BUILD_NASM_ACCEL OFF CACHE BOOL "NASM is not an Android target" FORCE)
set(VOX_BUILD_WIN32_LEGACY_DEMO OFF CACHE BOOL "Not an Android target" FORCE)
add_subdirectory("${VOX_ROOT}" "${CMAKE_BINARY_DIR}/vox")
