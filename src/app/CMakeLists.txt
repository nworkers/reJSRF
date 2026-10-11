add_executable(rejsrf main.cpp)
target_link_libraries(rejsrf PRIVATE rejsrf_game rexbox_hle)
target_compile_definitions(rejsrf PRIVATE REJSRF_VERSION="${REJSRF_VERSION}")
