# Title tools. rexbox's own tools and probes (rexbox_xbe_info,
# rexbox_xdk_symbols, the kernel, DSOUND and D3D8 probes) come with
# third_party/rexbox.

# Compares decompiled functions with the lifted bodies they replace; needs the
# generated code.
if(TARGET rejsrf_recomp)
    add_executable(rejsrf_decomp_check decomp_check/main.cpp)
    target_link_libraries(rejsrf_decomp_check PRIVATE rejsrf_game)
    rexbox_strict_warnings(rejsrf_decomp_check)

    # Compares the D3D8 HLE exports with the lifted XDK originals they
    # replace; needs the generated code and the user's XBE.
    add_executable(rejsrf_d3d8_check d3d8_check/main.cpp)
    target_link_libraries(rejsrf_d3d8_check PRIVATE rejsrf_game rexbox_hle_kernel rexbox_hle_d3d8)
    if(MSVC)
        target_link_options(rejsrf_d3d8_check PRIVATE /STACK:67108864)
    endif()
    rexbox_strict_warnings(rejsrf_d3d8_check)
endif()
