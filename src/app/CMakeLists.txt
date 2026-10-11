add_executable(rejsrf main.cpp)
target_link_libraries(rejsrf PRIVATE rejsrf_game rexbox_hle)
target_compile_definitions(rejsrf PRIVATE REJSRF_VERSION="${REJSRF_VERSION}")
if(MSVC)
    # Lifted guest calls nest as native C calls; Debug frames are large.
    target_link_options(rejsrf PRIVATE /STACK:67108864)
endif()
