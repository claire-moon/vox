cmake_minimum_required(VERSION 3.16)
project(vox VERSION 0.0.1 LANGUAGES C CXX)

option(VOX_BUILD_TESTS "Build deterministic VOX tests" ON)
option(VOX_BUILD_SDL2_DEMO "Build the playable SDL2 DIGS demo" OFF)
option(VOX_BUILD_NASM_ACCEL "Build the optional Linux x86-64 NASM contract probe" OFF)

if (VOX_BUILD_NASM_ACCEL)
    string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" VOX_SYSTEM_PROCESSOR_LOWER)
    if (NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR
        NOT VOX_SYSTEM_PROCESSOR_LOWER MATCHES "^(x86_64|amd64)$")
        message(FATAL_ERROR
            "VOX_BUILD_NASM_ACCEL supports only Linux System V x86-64; "
            "configure with -DVOX_BUILD_NASM_ACCEL=OFF on this target")
    endif()
    include(CheckLanguage)
    set(CMAKE_ASM_NASM_OBJECT_FORMAT elf64)
    check_language(ASM_NASM)
    if (NOT CMAKE_ASM_NASM_COMPILER)
        message(FATAL_ERROR
            "VOX_BUILD_NASM_ACCEL=ON requires the NASM assembler")
    endif()
    enable_language(ASM_NASM)
endif()

add_library(vox_accel STATIC
    engine/c89/vox_accel.c
)
target_include_directories(vox_accel PUBLIC engine/include)
set_property(TARGET vox_accel PROPERTY C_STANDARD 90)
set_property(TARGET vox_accel PROPERTY C_STANDARD_REQUIRED ON)
if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(vox_accel PRIVATE
        $<$<COMPILE_LANGUAGE:C>:-Wall>
        $<$<COMPILE_LANGUAGE:C>:-Wextra>
        $<$<COMPILE_LANGUAGE:C>:-Wpedantic>
        $<$<COMPILE_LANGUAGE:C>:-Werror>
    )
endif()
if (VOX_BUILD_NASM_ACCEL)
    target_sources(vox_accel PRIVATE engine/nasm/vox_fnv1a_x86_64.asm)
    target_compile_definitions(vox_accel PRIVATE VOX_HAVE_NASM_FNV1A=1)
endif()

add_library(vox_physics STATIC
    engine/cpp98/vox_physics.cpp
)
target_include_directories(vox_physics PUBLIC engine/include)
set_property(TARGET vox_physics PROPERTY CXX_STANDARD 98)
set_property(TARGET vox_physics PROPERTY CXX_STANDARD_REQUIRED ON)
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(vox_physics PRIVATE -Wall -Wextra -Wpedantic -Werror -fno-exceptions -fno-rtti)
endif()

add_library(vox_kernel STATIC
    engine/c89/vox_kernel.c
)
target_include_directories(vox_kernel PUBLIC engine/include)
set_property(TARGET vox_kernel PROPERTY C_STANDARD 90)
set_property(TARGET vox_kernel PROPERTY C_STANDARD_REQUIRED ON)
if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(vox_kernel PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

target_link_libraries(vox_physics PUBLIC vox_kernel)

add_library(digs_game STATIC
    games/digs/digs_game.c
)
target_link_libraries(digs_game PUBLIC vox_kernel vox_physics)
target_include_directories(digs_game PUBLIC engine/include)
set_property(TARGET digs_game PROPERTY C_STANDARD 90)
set_property(TARGET digs_game PROPERTY C_STANDARD_REQUIRED ON)
if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(digs_game PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_library(vox_software STATIC
    ports/software/vox_software.c
)
target_link_libraries(vox_software PUBLIC vox_kernel)
target_include_directories(vox_software PUBLIC engine/include)
set_property(TARGET vox_software PROPERTY C_STANDARD 90)
set_property(TARGET vox_software PROPERTY C_STANDARD_REQUIRED ON)
if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(vox_software PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_executable(vox_headless tools/vox_headless.c)
target_link_libraries(vox_headless PRIVATE vox_kernel)
target_include_directories(vox_headless PRIVATE engine/include)
if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(vox_headless PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_executable(digs_headless tools/digs_headless.c)
target_link_libraries(digs_headless PRIVATE digs_game)
target_include_directories(digs_headless PRIVATE engine/include)
set_property(TARGET digs_headless PROPERTY LINKER_LANGUAGE CXX)
if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(digs_headless PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_executable(vox_render_demo tools/vox_render_demo.c)
target_link_libraries(vox_render_demo PRIVATE digs_game vox_software)
target_include_directories(vox_render_demo PRIVATE engine/include)
set_property(TARGET vox_render_demo PROPERTY LINKER_LANGUAGE CXX)
if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(vox_render_demo PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

if (VOX_BUILD_SDL2_DEMO)
    find_package(SDL2 2.0.10 CONFIG REQUIRED)
    add_executable(digs_demo
        ports/sdl2/digs_sdl2.c
        ports/sdl2/vox_sdl_ui.c
    )
    target_link_libraries(digs_demo PRIVATE digs_game vox_software SDL2::SDL2)
    target_include_directories(digs_demo PRIVATE engine/include ports/sdl2)
    target_compile_definitions(digs_demo PRIVATE SDL_MAIN_HANDLED)
    set_property(TARGET digs_demo PROPERTY C_STANDARD 90)
    set_property(TARGET digs_demo PROPERTY C_STANDARD_REQUIRED ON)
    set_property(TARGET digs_demo PROPERTY C_EXTENSIONS OFF)
    set_property(TARGET digs_demo PROPERTY LINKER_LANGUAGE CXX)
    if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(digs_demo PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endif()

if (VOX_BUILD_TESTS)
    enable_testing()
    add_executable(vox_accel_test tests/headless/vox_accel_test.c)
    target_link_libraries(vox_accel_test PRIVATE vox_accel)
    target_include_directories(vox_accel_test PRIVATE engine/include)
    set_property(TARGET vox_accel_test PROPERTY C_STANDARD 90)
    set_property(TARGET vox_accel_test PROPERTY C_STANDARD_REQUIRED ON)
    if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(vox_accel_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME vox_accel_scalar_nasm_parity COMMAND vox_accel_test)
    add_executable(vox_kernel_test tests/headless/vox_kernel_test.c)
    target_link_libraries(vox_kernel_test PRIVATE vox_kernel)
    target_include_directories(vox_kernel_test PRIVATE engine/include)
    add_test(NAME vox_kernel_determinism COMMAND vox_kernel_test)
    add_executable(vox_physics_test tests/headless/vox_physics_test.cpp)
    target_link_libraries(vox_physics_test PRIVATE vox_physics vox_kernel)
    target_include_directories(vox_physics_test PRIVATE engine/include)
    set_property(TARGET vox_physics_test PROPERTY CXX_STANDARD 98)
    set_property(TARGET vox_physics_test PROPERTY CXX_STANDARD_REQUIRED ON)
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(vox_physics_test PRIVATE -Wall -Wextra -Wpedantic -Werror -fno-exceptions -fno-rtti)
    endif()
    add_test(NAME vox_physics_determinism COMMAND vox_physics_test)
    add_executable(vox_physics_c_abi_test tests/headless/vox_physics_c_abi_test.c)
    target_link_libraries(vox_physics_c_abi_test PRIVATE vox_physics)
    target_include_directories(vox_physics_c_abi_test PRIVATE engine/include)
    set_property(TARGET vox_physics_c_abi_test PROPERTY C_STANDARD 90)
    set_property(TARGET vox_physics_c_abi_test PROPERTY C_STANDARD_REQUIRED ON)
    set_property(TARGET vox_physics_c_abi_test PROPERTY LINKER_LANGUAGE CXX)
    if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(vox_physics_c_abi_test PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME vox_physics_c_abi COMMAND vox_physics_c_abi_test)
    add_executable(vox_game_test tests/headless/vox_game_test.c)
    target_link_libraries(vox_game_test PRIVATE digs_game)
    target_include_directories(vox_game_test PRIVATE engine/include)
    set_property(TARGET vox_game_test PROPERTY LINKER_LANGUAGE CXX)
    add_test(NAME digs_match_determinism COMMAND vox_game_test)
    add_executable(vox_software_test tests/headless/vox_software_test.c)
    target_link_libraries(vox_software_test PRIVATE vox_software)
    target_include_directories(vox_software_test PRIVATE engine/include)
    add_test(NAME vox_software_determinism COMMAND vox_software_test)
    add_test(NAME vox_headless_scenario COMMAND vox_headless)
    add_test(NAME digs_headless_scenario COMMAND digs_headless)
endif()
