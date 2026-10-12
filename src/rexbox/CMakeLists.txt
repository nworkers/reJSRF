# rexbox: the title-agnostic Xbox runtime. Nothing in this directory may name
# Jet Set Radio Future or include rejsrf/ headers; that is what lets another
# title reuse it unchanged.

# XBE image parsing. Pure data; no platform dependency.
add_library(rexbox_xbe STATIC
    xbe/xbe_image.cpp
)
target_include_directories(rexbox_xbe PUBLIC "${PROJECT_SOURCE_DIR}/include")

# Runtime core: guest memory, XBE placement, the HLE registry.
add_library(rexbox_runtime STATIC
    runtime/environment.cpp
    runtime/guest_heap.cpp
    runtime/guest_memory.cpp
    runtime/hle_call.cpp
    runtime/hle_registry.cpp
    runtime/xbe_loader.cpp
)
target_link_libraries(rexbox_runtime PUBLIC rexbox_xbe rexbox_platform)

# Recompiled-code ABI adapter: the only code tied to the lifter's ABI
# (xboxrecomp's recomp_types.h). A lifter upgrade changes this target alone.
add_library(rexbox_recomp STATIC
    recomp/recomp_abi.c
    recomp/recomp_bridge.cpp
    recomp/recomp_runtime.cpp
)
target_include_directories(rexbox_recomp PRIVATE "${REJSRF_XBOXRECOMP_DIR}/templates/runtime")
target_link_libraries(rexbox_recomp PUBLIC rexbox_runtime)

# HLE modules. Each links the runtime only, never another module, so any one
# can be replaced (or reused by another title) on its own.
add_library(rexbox_hle_kernel STATIC
    hle/kernel/kernel_crypto.cpp
    hle/kernel/kernel_data.cpp
    hle/kernel/kernel_exports.cpp
    hle/kernel/kernel_file.cpp
    hle/kernel/kernel_hal.cpp
    hle/kernel/kernel_hle.cpp
    hle/kernel/kernel_memory.cpp
    hle/kernel/kernel_object.cpp
    hle/kernel/kernel_rtl.cpp
    hle/kernel/kernel_state.cpp
    hle/kernel/kernel_sync.cpp
    hle/kernel/kernel_thread.cpp
    hle/kernel/kernel_vfs.cpp
)
target_link_libraries(rexbox_hle_kernel PUBLIC rexbox_runtime)

foreach(module IN ITEMS xapi dsound xinput)
    add_library(rexbox_hle_${module} STATIC hle/${module}/${module}_hle.cpp)
    target_link_libraries(rexbox_hle_${module} PUBLIC rexbox_runtime)
endforeach()

# Direct3D 8 on the platform display contract (OpenGL family). One file per
# area; d3d8_hle.cpp only registers.
add_library(rexbox_hle_d3d8 STATIC
    hle/d3d8/d3d8_adapter.cpp
    hle/d3d8/d3d8_caps.cpp
    hle/d3d8/d3d8_device.cpp
    hle/d3d8/d3d8_gl.cpp
    hle/d3d8/d3d8_hle.cpp
)
target_link_libraries(rexbox_hle_d3d8 PUBLIC rexbox_runtime)

add_library(rexbox_hle INTERFACE)
target_link_libraries(rexbox_hle INTERFACE
    rexbox_hle_kernel rexbox_hle_xapi rexbox_hle_d3d8 rexbox_hle_dsound rexbox_hle_xinput)
