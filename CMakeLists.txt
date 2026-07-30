cmake_minimum_required(VERSION 3.16)
project(vox VERSION 0.0.3 LANGUAGES C CXX)

# The portable scalar oracle is compiled as ISO C90 (the C89 language profile)
# and the body sidecar as ISO C++98.  Keep compiler extensions off globally so
# every target, including small tools and tests, exercises the same boundary.
set(CMAKE_C_STANDARD 90)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 98)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(VOX_BUILD_TESTS "Build deterministic VOX tests" ON)
option(VOX_BUILD_SDL2_DEMO "Build the playable SDL2 DIGS demo" OFF)
option(VOX_BUILD_NASM_ACCEL "Build the optional Linux x86-64 NASM contract probe" OFF)
option(VOX_BUILD_LEGACY_PROFILE
       "Build the reduced-memory 400x240x4 VOX compatibility profile" OFF)
option(VOX_BUILD_WIN32_LEGACY_DEMO
       "Build the native GDI/WinMM PE32 DIGS compatibility demo" OFF)

# The SDL2 host is deliberately not used by the historical Windows target:
# SDL2 no longer supports the Windows 9x / NT4 baseline.  The legacy host has
# a separately compiled core so its reduced world dimensions cannot change the
# ABI of current desktop builds.
if (VOX_BUILD_WIN32_LEGACY_DEMO)
    if (NOT WIN32)
        message(FATAL_ERROR
            "VOX_BUILD_WIN32_LEGACY_DEMO requires a Windows cross/native toolchain")
    endif()
    if (NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
        message(FATAL_ERROR
            "VOX_BUILD_WIN32_LEGACY_DEMO requires a 32-bit PE32 toolchain")
    endif()
    if (NOT MINGW OR NOT CMAKE_C_COMPILER_ID STREQUAL "GNU" OR
        NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        message(FATAL_ERROR
            "VOX_BUILD_WIN32_LEGACY_DEMO requires the i686 MinGW GNU C/C++ toolchain")
    endif()
    set(VOX_BUILD_LEGACY_PROFILE ON)
endif()

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

add_library(vox_audio STATIC
    engine/c89/vox_audio.c
)
target_include_directories(vox_audio PUBLIC engine/include)
set_property(TARGET vox_audio PROPERTY C_STANDARD 90)
set_property(TARGET vox_audio PROPERTY C_STANDARD_REQUIRED ON)
if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(vox_audio PRIVATE
        $<$<COMPILE_LANGUAGE:C>:-Wall>
        $<$<COMPILE_LANGUAGE:C>:-Wextra>
        $<$<COMPILE_LANGUAGE:C>:-Wpedantic>
        $<$<COMPILE_LANGUAGE:C>:-Werror>
    )
endif()

# Lua 5.1.5 remains under its upstream MIT license.  The build intentionally
# excludes the standalone interpreters and the io/os/package/debug libraries;
# VOX exposes only its deterministic catalog sandbox through vox_script.
add_library(lua51 STATIC
    third_party/lua-5.1.5/src/lapi.c
    third_party/lua-5.1.5/src/lcode.c
    third_party/lua-5.1.5/src/ldebug.c
    third_party/lua-5.1.5/src/ldo.c
    third_party/lua-5.1.5/src/ldump.c
    third_party/lua-5.1.5/src/lfunc.c
    third_party/lua-5.1.5/src/lgc.c
    third_party/lua-5.1.5/src/llex.c
    third_party/lua-5.1.5/src/lmem.c
    third_party/lua-5.1.5/src/lobject.c
    third_party/lua-5.1.5/src/lopcodes.c
    third_party/lua-5.1.5/src/lparser.c
    third_party/lua-5.1.5/src/lstate.c
    third_party/lua-5.1.5/src/lstring.c
    third_party/lua-5.1.5/src/ltable.c
    third_party/lua-5.1.5/src/ltm.c
    third_party/lua-5.1.5/src/lundump.c
    third_party/lua-5.1.5/src/lvm.c
    third_party/lua-5.1.5/src/lzio.c
    third_party/lua-5.1.5/src/lauxlib.c
    third_party/lua-5.1.5/src/lbaselib.c
    third_party/lua-5.1.5/src/ltablib.c
    third_party/lua-5.1.5/src/lstrlib.c
    third_party/lua-5.1.5/src/lmathlib.c
)
target_include_directories(lua51 PUBLIC third_party/lua-5.1.5/src)
target_compile_definitions(lua51 PRIVATE LUA_ANSI=1)
set_property(TARGET lua51 PROPERTY C_STANDARD 90)
set_property(TARGET lua51 PROPERTY C_STANDARD_REQUIRED ON)
if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    # Preserve upstream 5.1.5 source byte-for-byte; modern diagnostics in that
    # third-party code are not made project errors.
    target_compile_options(lua51 PRIVATE -w)
endif()
if (NOT MSVC)
    target_link_libraries(lua51 PUBLIC m)
endif()

add_library(vox_script STATIC
    engine/c89/vox_script.c
)
target_include_directories(vox_script PUBLIC engine/include)
target_include_directories(vox_script PRIVATE third_party/lua-5.1.5/src)
target_link_libraries(vox_script PUBLIC lua51)
set_property(TARGET vox_script PROPERTY C_STANDARD 90)
set_property(TARGET vox_script PROPERTY C_STANDARD_REQUIRED ON)
if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(vox_script PRIVATE
        -Wall -Wextra -Wpedantic -Werror)
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

add_library(digs_miner_art STATIC
    ports/software/digs_miner_art.c
)
target_link_libraries(digs_miner_art PUBLIC vox_software)
target_include_directories(digs_miner_art PUBLIC
    engine/include
    ports/software
)
set_property(TARGET digs_miner_art PROPERTY C_STANDARD 90)
set_property(TARGET digs_miner_art PROPERTY C_STANDARD_REQUIRED ON)
if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(digs_miner_art PRIVATE
        -Wall -Wextra -Wpedantic -Werror)
endif()

# ---------------------------------------------------------------------------
# Historical Win32 profile
#
# This is intentionally a distinct set of static libraries.  World dimensions
# are part of public struct layouts, therefore changing them on vox_kernel
# would silently corrupt every normal desktop target that links it.
# ---------------------------------------------------------------------------
if (VOX_BUILD_LEGACY_PROFILE)
    set(VOX_LEGACY_WORLD_DEFINITIONS
        VOX_WORLD_WIDTH=400U
        VOX_WORLD_HEIGHT=240U
        VOX_WORLD_DEPTH=4U
    )
    if (CMAKE_SIZEOF_VOID_P EQUAL 4 AND CMAKE_C_COMPILER_ID MATCHES "GNU")
        set(VOX_LEGACY_C_CPU_OPTIONS
            -march=i386 -mno-sse -mno-sse2 -mfpmath=387
        )
    endif()
    if (CMAKE_SIZEOF_VOID_P EQUAL 4 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        set(VOX_LEGACY_CXX_CPU_OPTIONS
            -march=i386 -mno-sse -mno-sse2 -mfpmath=387
        )
    endif()

    # The WinMM host owns its own i386-safe synth object.  Reusing vox_audio
    # here would make it too easy for a future desktop compiler default to
    # introduce an instruction unavailable on the target CPU baseline.
    add_library(vox_legacy_audio STATIC
        engine/c89/vox_audio.c
    )
    target_include_directories(vox_legacy_audio PUBLIC engine/include)
    set_property(TARGET vox_legacy_audio PROPERTY C_STANDARD 90)
    set_property(TARGET vox_legacy_audio PROPERTY C_STANDARD_REQUIRED ON)
    if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(vox_legacy_audio PRIVATE
            -Wall -Wextra -Wpedantic -Werror
            ${VOX_LEGACY_C_CPU_OPTIONS})
    endif()

    add_library(vox_legacy_kernel STATIC
        engine/c89/vox_kernel.c
    )
    target_include_directories(vox_legacy_kernel PUBLIC engine/include)
    target_compile_definitions(vox_legacy_kernel PUBLIC
        ${VOX_LEGACY_WORLD_DEFINITIONS})
    set_property(TARGET vox_legacy_kernel PROPERTY C_STANDARD 90)
    set_property(TARGET vox_legacy_kernel PROPERTY C_STANDARD_REQUIRED ON)
    if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(vox_legacy_kernel PRIVATE
            -Wall -Wextra -Wpedantic -Werror
            ${VOX_LEGACY_C_CPU_OPTIONS})
    endif()

    add_library(vox_legacy_physics STATIC
        engine/cpp98/vox_physics.cpp
    )
    target_include_directories(vox_legacy_physics PUBLIC engine/include)
    target_compile_definitions(vox_legacy_physics PUBLIC
        ${VOX_LEGACY_WORLD_DEFINITIONS})
    target_link_libraries(vox_legacy_physics PUBLIC vox_legacy_kernel)
    set_property(TARGET vox_legacy_physics PROPERTY CXX_STANDARD 98)
    set_property(TARGET vox_legacy_physics PROPERTY CXX_STANDARD_REQUIRED ON)
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(vox_legacy_physics PRIVATE
            -Wall -Wextra -Wpedantic -Werror -fno-exceptions -fno-rtti
            ${VOX_LEGACY_CXX_CPU_OPTIONS})
    endif()

    add_library(digs_legacy_game STATIC
        games/digs/digs_game.c
    )
    target_include_directories(digs_legacy_game PUBLIC engine/include)
    target_compile_definitions(digs_legacy_game PUBLIC
        ${VOX_LEGACY_WORLD_DEFINITIONS})
    target_link_libraries(digs_legacy_game PUBLIC
        vox_legacy_kernel vox_legacy_physics)
    set_property(TARGET digs_legacy_game PROPERTY C_STANDARD 90)
    set_property(TARGET digs_legacy_game PROPERTY C_STANDARD_REQUIRED ON)
    if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(digs_legacy_game PRIVATE
            -Wall -Wextra -Wpedantic -Werror
            ${VOX_LEGACY_C_CPU_OPTIONS})
    endif()

    add_library(vox_legacy_software STATIC
        ports/software/vox_software.c
    )
    target_include_directories(vox_legacy_software PUBLIC engine/include)
    target_compile_definitions(vox_legacy_software PUBLIC
        ${VOX_LEGACY_WORLD_DEFINITIONS})
    target_link_libraries(vox_legacy_software PUBLIC vox_legacy_kernel)
    set_property(TARGET vox_legacy_software PROPERTY C_STANDARD 90)
    set_property(TARGET vox_legacy_software PROPERTY C_STANDARD_REQUIRED ON)
    if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(vox_legacy_software PRIVATE
            -Wall -Wextra -Wpedantic -Werror
            ${VOX_LEGACY_C_CPU_OPTIONS})
    endif()
endif()

if (VOX_BUILD_WIN32_LEGACY_DEMO)
    add_executable(digs_win32_legacy WIN32
        ports/win32/digs_win32_legacy.c
        ports/sdl2/vox_sdl_ui.c
    )
    target_include_directories(digs_win32_legacy PRIVATE
        engine/include
        ports/sdl2
    )
    target_compile_definitions(digs_win32_legacy PRIVATE
        ${VOX_LEGACY_WORLD_DEFINITIONS}
        WINVER=0x0400
        _WIN32_WINNT=0x0400
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
    target_link_libraries(digs_win32_legacy PRIVATE
        digs_legacy_game
        vox_legacy_software
        vox_legacy_audio
        user32
        gdi32
        winmm
    )
    set_property(TARGET digs_win32_legacy PROPERTY C_STANDARD 90)
    set_property(TARGET digs_win32_legacy PROPERTY C_STANDARD_REQUIRED ON)
    set_property(TARGET digs_win32_legacy PROPERTY C_EXTENSIONS OFF)
    set_property(TARGET digs_win32_legacy PROPERTY LINKER_LANGUAGE CXX)
    if (CMAKE_C_COMPILER_ID MATCHES "GNU")
        target_compile_options(digs_win32_legacy PRIVATE
            -Wall -Wextra -Wpedantic -Werror
            ${VOX_LEGACY_C_CPU_OPTIONS})
        target_link_options(digs_win32_legacy PRIVATE
            -static -static-libgcc -static-libstdc++
            -Wl,--subsystem,windows
            -Wl,--major-os-version,4
            -Wl,--minor-os-version,0
            -Wl,--major-subsystem-version,4
            -Wl,--minor-subsystem-version,0)
    endif()
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

if (VOX_BUILD_LEGACY_PROFILE)
    # The legacy profile has intentionally different dimensions and therefore
    # different golden hashes.  This scenario verifies its real ABI/step path
    # without weakening the desktop deterministic fixtures above.
    add_executable(digs_legacy_headless tools/digs_headless.c)
    target_link_libraries(digs_legacy_headless PRIVATE digs_legacy_game)
    target_include_directories(digs_legacy_headless PRIVATE engine/include)
    target_compile_definitions(digs_legacy_headless PRIVATE
        ${VOX_LEGACY_WORLD_DEFINITIONS})
    set_property(TARGET digs_legacy_headless PROPERTY LINKER_LANGUAGE CXX)
    if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(digs_legacy_headless PRIVATE
            -Wall -Wextra -Wpedantic -Werror)
    endif()
endif()

add_executable(digs_script tools/digs_script.c)
target_link_libraries(digs_script PRIVATE vox_script)
target_include_directories(digs_script PRIVATE engine/include)
set_property(TARGET digs_script PROPERTY C_STANDARD 90)
set_property(TARGET digs_script PROPERTY C_STANDARD_REQUIRED ON)
if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(digs_script PRIVATE
        -Wall -Wextra -Wpedantic -Werror)
endif()

file(COPY games/digs/scripts DESTINATION "${CMAKE_BINARY_DIR}/share/digs")
file(COPY games/digs/catalog DESTINATION "${CMAKE_BINARY_DIR}/share/digs")
file(COPY games/digs/icons DESTINATION "${CMAKE_BINARY_DIR}/share/digs")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/share/digs/controllers")
configure_file(
    "${CMAKE_SOURCE_DIR}/third_party/SDL_GameControllerDB/gamecontrollerdb.txt"
    "${CMAKE_BINARY_DIR}/share/digs/controllers/gamecontrollerdb.txt"
    COPYONLY
)

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
    target_link_libraries(digs_demo PRIVATE digs_game digs_miner_art vox_audio vox_script SDL2::SDL2)
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
    add_executable(vox_audio_test tests/headless/vox_audio_test.c)
    target_link_libraries(vox_audio_test PRIVATE vox_audio)
    target_include_directories(vox_audio_test PRIVATE engine/include)
    set_property(TARGET vox_audio_test PROPERTY C_STANDARD 90)
    set_property(TARGET vox_audio_test PROPERTY C_STANDARD_REQUIRED ON)
    if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(vox_audio_test PRIVATE
            -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME vox_audio_determinism COMMAND vox_audio_test)
    add_executable(vox_script_test tests/headless/vox_script_test.c)
    target_link_libraries(vox_script_test PRIVATE vox_script)
    target_include_directories(vox_script_test PRIVATE engine/include)
    set_property(TARGET vox_script_test PROPERTY C_STANDARD 90)
    set_property(TARGET vox_script_test PROPERTY C_STANDARD_REQUIRED ON)
    if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(vox_script_test PRIVATE
            -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME vox_script_determinism COMMAND vox_script_test)
    set_tests_properties(vox_script_determinism PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
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
    add_executable(digs_miner_art_test
        tests/headless/digs_miner_art_test.c)
    target_link_libraries(digs_miner_art_test PRIVATE digs_miner_art)
    target_include_directories(digs_miner_art_test PRIVATE
        engine/include
        ports/software
    )
    if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(digs_miner_art_test PRIVATE
            -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME digs_miner_icon_determinism COMMAND digs_miner_art_test)
    if (TARGET digs_demo)
        add_test(NAME digs_input_normalization
            COMMAND digs_demo --input-self-test)
        add_test(NAME digs_cap_qualification
            COMMAND digs_demo --cap-self-test)
        add_test(NAME digs_audio_cadence
            COMMAND digs_demo --audio-cadence-self-test)
        add_test(NAME digs_bark_library
            COMMAND digs_demo --bark-self-test)
        add_test(NAME digs_haptic_mixer
            COMMAND digs_demo --haptic-self-test)
        add_test(NAME digs_deterministic_load
            COMMAND digs_demo --load-self-test 600)
        add_test(NAME digs_settings_migration
            COMMAND digs_demo --settings-self-test
                    ${CMAKE_CURRENT_BINARY_DIR}/digs-settings-self-test.cfg)
        add_test(NAME digs_camera_bounds
            COMMAND digs_demo --camera-self-test)
        add_test(NAME digs_fixed_step_debt
            COMMAND digs_demo --fixed-step-self-test)
    endif()
    add_test(NAME vox_headless_scenario COMMAND vox_headless)
    add_test(NAME digs_headless_scenario COMMAND digs_headless)
    if (TARGET digs_legacy_headless)
        add_test(NAME digs_legacy_headless_scenario COMMAND digs_legacy_headless)
    endif()
endif()
