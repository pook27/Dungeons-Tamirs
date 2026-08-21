cmake_minimum_required(VERSION 3.15)
project(dungeons_and_tamirs C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# --- raylib ---------------------------------------------------------------
# Prefer a raylib already installed on the system (e.g. via vcpkg on Windows,
# Homebrew on macOS, or your distro's package manager); fall back to fetching
# and building it from source so `cmake --build .` works with nothing pre-installed.
find_package(raylib 4.5 QUIET)
if (NOT raylib_FOUND)
    include(FetchContent)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG 5.5
    )
    FetchContent_MakeAvailable(raylib)
endif()

add_executable(dungeons_and_tamirs
    main.c
    assets.c
    world.c
    entities.c
    upgrades.c
    ui.c
)

target_link_libraries(dungeons_and_tamirs PRIVATE raylib)

# raylib's own per-platform system libs (OpenGL/GDI/etc.) - mirrors raylib's
# own CMake examples, needed on top of the raylib target itself.
if (WIN32)
    target_link_libraries(dungeons_and_tamirs PRIVATE winmm gdi32 opengl32)
elseif (APPLE)
    target_link_libraries(dungeons_and_tamirs PRIVATE "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
elseif (UNIX)
    target_link_libraries(dungeons_and_tamirs PRIVATE m pthread dl rt X11)
endif()

# Windows: keep the console window (useful for printf/debug output during dev).
# Switch WIN32_EXECUTABLE to TRUE later if you want a windowed-only release build.
set_target_properties(dungeons_and_tamirs PROPERTIES WIN32_EXECUTABLE FALSE)

# Copy the assets/ folder next to the built executable so LoadTexture("assets/...")
# keeps working unmodified on every platform - MSVC and multi-config generators
# put the binary in a config subfolder, so this targets $<TARGET_FILE_DIR:...>.
add_custom_command(TARGET dungeons_and_tamirs POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:dungeons_and_tamirs>/assets
)
