# rexbox_platform: the OS boundary. Contracts are in include/rexbox/platform/;
# each platform directory implements them. Knows no runtime or title types.
set(REXBOX_PLATFORM_SOURCES_win32
    win32/guarded_call_win32.cpp
    win32/host_thread_win32.cpp
    win32/virtual_memory_win32.cpp
)

add_library(rexbox_platform STATIC ${REXBOX_PLATFORM_SOURCES_${REJSRF_PLATFORM}})
target_include_directories(rexbox_platform PUBLIC "${PROJECT_SOURCE_DIR}/include")
