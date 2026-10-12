add_executable(rexbox_xbe_info xbe_info/main.cpp)
target_link_libraries(rexbox_xbe_info PRIVATE rexbox_xbe rexbox_hle_kernel)

# XDK symbol identification for regenerate.py.
add_executable(rexbox_xdk_symbols xdk_symbols/main.cpp)
target_link_libraries(rexbox_xdk_symbols PRIVATE libXbSymbolDatabase)

# Kernel HLE checks that need no game data (run in CI).
add_executable(rexbox_kernel_probe kernel_probe/main.cpp)
target_link_libraries(rexbox_kernel_probe PRIVATE rexbox_hle_kernel)
rejsrf_check_license(rexbox_kernel_probe)
rejsrf_strict_warnings(rexbox_kernel_probe)

# Compares decompiled functions with the lifted bodies they replace; needs the
# generated code.
if(TARGET rejsrf_recomp)
    add_executable(rejsrf_decomp_check decomp_check/main.cpp)
    target_link_libraries(rejsrf_decomp_check PRIVATE rejsrf_game)
    rejsrf_check_license(rejsrf_decomp_check)
    rejsrf_strict_warnings(rejsrf_decomp_check)

    # Compares the D3D8 HLE exports with the lifted XDK originals they
    # replace; needs the generated code and the user's XBE.
    add_executable(rejsrf_d3d8_check d3d8_check/main.cpp)
    target_link_libraries(rejsrf_d3d8_check PRIVATE rejsrf_game rexbox_hle_kernel rexbox_hle_d3d8)
    if(MSVC)
        target_link_options(rejsrf_d3d8_check PRIVATE /STACK:67108864)
    endif()
    rejsrf_check_license(rejsrf_d3d8_check)
    rejsrf_strict_warnings(rejsrf_d3d8_check)
endif()
