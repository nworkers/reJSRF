add_executable(rejsrf main.cpp)
target_link_libraries(rejsrf PRIVATE rejsrf_game rexbox_hle)
target_compile_definitions(rejsrf PRIVATE REJSRF_VERSION="${REJSRF_VERSION}")
if(MSVC)
    # Guest threads get their 64 MiB host stacks explicitly (CreateHostThread);
    # the image default stays small because every other thread in the process
    # (audio pacing, SDL, the GL driver's workers) reserves it too, which used
    # to exhaust the 32-bit address space. Large-address-aware gives the
    # process 4 GiB on 64-bit Windows.
    target_link_options(rejsrf PRIVATE /LARGEADDRESSAWARE)
endif()
