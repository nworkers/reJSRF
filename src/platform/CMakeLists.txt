# rexbox_platform: the OS boundary. Contracts are in include/rexbox/platform/;
# each OS directory implements them, and backends on a host library several
# OSes share (SDL3) live in a directory named after the library and are listed
# by each OS that uses them. Knows no runtime or title types.
set(REXBOX_PLATFORM_SOURCES_win32
    win32/guarded_call_win32.cpp
    win32/host_thread_win32.cpp
    win32/virtual_memory_win32.cpp
    sdl/display_sdl.cpp
    sdl/input_sdl.cpp
)

add_library(rexbox_platform STATIC ${REXBOX_PLATFORM_SOURCES_${REJSRF_PLATFORM}})
target_include_directories(rexbox_platform PUBLIC "${PROJECT_SOURCE_DIR}/include")
# SDL is an implementation detail of the platform layer: its headers and
# types never reach the targets above.
target_link_libraries(rexbox_platform PRIVATE SDL3::SDL3-static)
