cmake_minimum_required(VERSION 3.20)

project(reJSRF LANGUAGES C CXX)

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

# The shared Xbox runtime (third_party/rexbox, submodule reexec/rexbox):
# platform layer, XBE, runtime, lifter ABI adapter, HLE modules, tools and
# probes. Its consumer contract is in its ARCHITECTURE.md section 2
# (docs/design/20261022-i102-rexbox-submodule.md).
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/rexbox/CMakeLists.txt")
    message(FATAL_ERROR "third_party/rexbox is missing; run: git submodule update --init --recursive")
endif()
# The lifter is this project's: it pins its own xboxrecomp fork
# (third_party/xboxrecomp, branch rejsrf), and scripts/recomp/lifter.py runs it.
# The generated code carries the lifter's ABI header
# (gen/<variant>/gen/recomp_types.h), which rexbox_add_recompiled_library
# compiles rexbox's side of the ABI against.
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/xboxrecomp/templates/runtime/recomp_types.h")
    message(FATAL_ERROR "third_party/xboxrecomp is missing; run: git submodule update --init --recursive")
endif()
# rexbox_xdk_symbols (for scripts/recomp/regenerate.py) and the probes (CI).
set(REXBOX_BUILD_TOOLS ON CACHE BOOL "Build rexbox_xdk_symbols and the HLE probes")
add_subdirectory(third_party/rexbox)
# The compiler settings every target of the program shares (/EHa above all).
rexbox_apply_host_flags()
message(STATUS "reJSRF platform: ${REXBOX_PLATFORM}/${REXBOX_ARCH}")

# The XBE build this tree is built for: the USA or the Japanese disc. One
# executable runs one build; the other is a separate build directory
# (docs/design/20261020-i108-japanese-variant.md).
set(REJSRF_VARIANT "jsrf_us" CACHE STRING "XBE build to build for: jsrf_us (USA) or jsrf_jp (Japan)")
set_property(CACHE REJSRF_VARIANT PROPERTY STRINGS jsrf_us jsrf_jp)
if(NOT REJSRF_VARIANT MATCHES "^(jsrf_us|jsrf_jp)$")
    message(FATAL_ERROR "REJSRF_VARIANT must be jsrf_us or jsrf_jp, not '${REJSRF_VARIANT}'")
endif()
message(STATUS "reJSRF variant: ${REJSRF_VARIANT}")

# Lifted C for that variant, produced by scripts/recomp/regenerate.py.
# Without it the game links no recompiled code and the app stops after load.
set(REJSRF_RECOMP_GEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/gen/${REJSRF_VARIANT}/gen" CACHE PATH
    "Directory of generated recompiled C (recomp_dispatch.c and friends)")
# A tree reconfigured for the other variant still caches the old default.
if(REJSRF_RECOMP_GEN_DIR MATCHES "/gen/jsrf_(us|jp)/gen$"
   AND NOT REJSRF_RECOMP_GEN_DIR STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}/gen/${REJSRF_VARIANT}/gen")
    set(REJSRF_RECOMP_GEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/gen/${REJSRF_VARIANT}/gen" CACHE PATH
        "Directory of generated recompiled C (recomp_dispatch.c and friends)" FORCE)
endif()

# Tests: rexbox's probes (registered by the submodule); `ctest -C <config>`
# runs them from the build tree.
enable_testing()

# Layers above rexbox. Each links only the layers below it, which is what
# keeps the reusable Xbox runtime free of any title code.
add_subdirectory(src/game/jsrf)      # rejsrf_game (+ rejsrf_recomp): JSRF title code + profile
add_subdirectory(src/app)            # rejsrf: the executable
add_subdirectory(tools)              # title tools

foreach(target IN ITEMS rejsrf_game rejsrf)
    rexbox_strict_warnings(${target})
endforeach()
