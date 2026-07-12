cmake_minimum_required(VERSION 3.16)
project(vox VERSION 0.0.1 LANGUAGES C CXX)

option(VOX_BUILD_TESTS "Build deterministic VOX tests" ON)

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

add_library(digs_game STATIC
    games/digs/digs_game.c
)
target_link_libraries(digs_game PUBLIC vox_kernel)
target_include_directories(digs_game PUBLIC engine/include)
set_property(TARGET digs_game PROPERTY C_STANDARD 90)
set_property(TARGET digs_game PROPERTY C_STANDARD_REQUIRED ON)
if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(digs_game PRIVATE -Wall -Wextra -Wpedantic -Werror)
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
if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(digs_headless PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

if (VOX_BUILD_TESTS)
    enable_testing()
    add_executable(vox_kernel_test tests/headless/vox_kernel_test.c)
    target_link_libraries(vox_kernel_test PRIVATE vox_kernel)
    target_include_directories(vox_kernel_test PRIVATE engine/include)
    add_test(NAME vox_kernel_determinism COMMAND vox_kernel_test)
    add_executable(vox_game_test tests/headless/vox_game_test.c)
    target_link_libraries(vox_game_test PRIVATE digs_game)
    target_include_directories(vox_game_test PRIVATE engine/include)
    add_test(NAME digs_match_determinism COMMAND vox_game_test)
endif()
