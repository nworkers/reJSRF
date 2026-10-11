cmake_minimum_required(VERSION 3.20)

project(reJSRF LANGUAGES C CXX)

# Target platform and architecture. Platform code lives in
# src/platform/<platform>/ (and <platform>/<arch>/ when it differs by
# architecture); the runtime, HLE and title directories have none (AGENTS.md).
# Only win32/x86 is built and verified so far; the others are listed so the
# selection logic exists before their platform layers do.
if(EMSCRIPTEN)
    set(REJSRF_PLATFORM web)
    set(REJSRF_ARCH wasm32)
elseif(ANDROID)
    set(REJSRF_PLATFORM android)
    set(REJSRF_ARCH ${CMAKE_ANDROID_ARCH_ABI})
elseif(WIN32)
    set(REJSRF_PLATFORM win32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(REJSRF_ARCH x86)
    else()
        set(REJSRF_ARCH x64)
    endif()
elseif(UNIX)
    set(REJSRF_PLATFORM linux)
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(REJSRF_ARCH x86)
    else()
        set(REJSRF_ARCH x64)
    endif()
else()
    message(FATAL_ERROR "reJSRF targets Win32, Win64, Linux, the web and Android only")
endif()

if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/platform/${REJSRF_PLATFORM}")
    message(FATAL_ERROR
        "Platform '${REJSRF_PLATFORM}' has no platform layer yet (src/platform/${REJSRF_PLATFORM}/). "
        "Win32 x86 is the first target; see docs/PROJECT_CHARTER.md.")
endif()
message(STATUS "reJSRF platform: ${REJSRF_PLATFORM}/${REJSRF_ARCH}")

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/VERSION"
)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" REJSRF_VERSION LIMIT_COUNT 1)
string(STRIP "${REJSRF_VERSION}" REJSRF_VERSION)
if(NOT REJSRF_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "VERSION must use major.minor.patch format")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(WIN32)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
if(MSVC)
    # /utf-8: sources are UTF-8; without it MSVC reads them in the system code
    # page (C4819 on a Korean locale).
    add_compile_options(/utf-8 /W4 /permissive-)
endif()

# License guard. third_party/xboxrecomp is MIT, except its xemu-derived files,
# which are LGPL-2.1+ (its NOTICE). Those must never be compiled into this
# project: we take xboxrecomp's tools and MIT sources only, file by file.
# Any source list that names one of these fails the configure.
set(REJSRF_COPYLEFT_PATHS
    "third_party/xboxrecomp/src/apu/"
    "third_party/xboxrecomp/src/nv2a/nv2a_core.c"
    "third_party/xboxrecomp/src/nv2a/nv2a_regs.h"
    "third_party/xboxrecomp/src/nv2a/nv2a_state.h"
    "third_party/xboxrecomp/include/xbox/xboxrecomp.h"
)
function(rejsrf_check_license target)
    get_target_property(sources ${target} SOURCES)
    get_target_property(source_dir ${target} SOURCE_DIR)
    foreach(source IN LISTS sources)
        cmake_path(ABSOLUTE_PATH source BASE_DIRECTORY "${source_dir}" NORMALIZE OUTPUT_VARIABLE absolute)
        file(RELATIVE_PATH relative "${CMAKE_SOURCE_DIR}" "${absolute}")
        foreach(blocked IN LISTS REJSRF_COPYLEFT_PATHS)
            string(FIND "${relative}" "${blocked}" hit)
            if(hit EQUAL 0)
                message(FATAL_ERROR "${target}: ${relative} is copyleft-licensed and may not be built (AGENTS.md)")
            endif()
        endforeach()
    endforeach()
endfunction()

# Layers, lowest first. Each layer links only the layers below it, which is
# what keeps the reusable Xbox runtime (rexbox) free of any title code.
add_subdirectory(src/platform)       # rexbox_platform: OS contracts
add_subdirectory(src/rexbox)         # rexbox_*: XBE, runtime, HLE modules
add_subdirectory(src/game/jsrf)      # rejsrf_game: JSRF title code + profile
add_subdirectory(src/app)            # rejsrf: the executable
add_subdirectory(tools)              # developer tools

foreach(target IN ITEMS
        rexbox_platform rexbox_xbe rexbox_runtime
        rexbox_hle_kernel rexbox_hle_xapi rexbox_hle_d3d8 rexbox_hle_dsound rexbox_hle_xinput
        rejsrf_game rejsrf rexbox_xbe_info)
    rejsrf_check_license(${target})
endforeach()
