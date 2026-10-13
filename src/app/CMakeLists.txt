add_executable(rejsrf main.cpp)
target_link_libraries(rejsrf PRIVATE rejsrf_game rexbox_hle)
target_compile_definitions(rejsrf PRIVATE REJSRF_VERSION="${REJSRF_VERSION}" REJSRF_BUILD_CONFIG="$<CONFIG>")

# The build time in the window title, refreshed on every build.
set(REJSRF_BUILD_STAMP "${CMAKE_CURRENT_BINARY_DIR}/generated/rejsrf_build_stamp.h")
add_custom_target(rejsrf_build_stamp
    COMMAND "${CMAKE_COMMAND}" "-DOUTPUT=${REJSRF_BUILD_STAMP}" -P "${CMAKE_CURRENT_SOURCE_DIR}/build_stamp.cmake"
    BYPRODUCTS "${REJSRF_BUILD_STAMP}"
    VERBATIM)
add_dependencies(rejsrf rejsrf_build_stamp)
target_include_directories(rejsrf PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
if(MSVC)
    # Guest threads get their 64 MiB host stacks explicitly (CreateHostThread);
    # the image default stays small because every other thread in the process
    # (audio pacing, SDL, the GL driver's workers) reserves it too, which used
    # to exhaust the 32-bit address space. Large-address-aware gives the
    # process 4 GiB on 64-bit Windows.
    target_link_options(rejsrf PRIVATE /LARGEADDRESSAWARE)
    # rejsrf.map: function addresses for scripts/perf/resolve_samples.py
    # (REXBOX_SAMPLE); the build has no PDB.
    target_link_options(rejsrf PRIVATE /MAP)
endif()
