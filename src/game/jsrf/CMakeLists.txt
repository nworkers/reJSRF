# rejsrf_game: everything specific to Jet Set Radio Future.
#
#   jsrf_profile.cpp   identity of supported XBE builds, HLE address bindings
#   jsrf_recomp.cpp    the recompiled entry point, when generated code exists
#   jsrf_host.c        the host hooks the decompiled sources call (include/rejsrf/jsrf_host.h)
#   decomp/            hand-decompiled functions that replace lifted ones
#   <gen dir>          lifted C from scripts/recomp/regenerate.py (gitignored)
file(GLOB REJSRF_DECOMP_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/decomp/*.c")

add_library(rejsrf_game STATIC
    jsrf_profile.cpp
    jsrf_recomp.cpp
    jsrf_host.c
    ${REJSRF_DECOMP_SOURCES}
)
target_include_directories(rejsrf_game PUBLIC "${PROJECT_SOURCE_DIR}/include")  # rejsrf/*.h
target_link_libraries(rejsrf_game PUBLIC rexbox_recomp)
# The build variant (root CMakeLists.txt): the profile and the decompiled
# sources pick its data addresses (include/rejsrf/jsrf_addresses.h).
if(REJSRF_VARIANT STREQUAL "jsrf_jp")
    target_compile_definitions(rejsrf_game PUBLIC REJSRF_VARIANT_JP=1)
elseif(REJSRF_VARIANT STREQUAL "jsrf_bundle_kor")
    target_compile_definitions(rejsrf_game PUBLIC REJSRF_VARIANT_BUNDLE_KOR=1)
endif()

# The generated code (rejsrf_recomp), built by rexbox against the pinned
# xboxrecomp's ABI header; nothing when the directory is empty (CI). Options:
# REXBOX_RECOMP_CALL_PROFILE, REXBOX_RECOMP_ABI_CHECK (rexbox's
# cmake/rexbox_recomp.cmake).
rexbox_add_recompiled_library(rejsrf_recomp GEN_DIR "${REJSRF_RECOMP_GEN_DIR}" TITLE rejsrf_game)
if(TARGET rejsrf_recomp)
    # Decompiled sources include the generated declarations.
    target_include_directories(rejsrf_game PRIVATE "${REJSRF_RECOMP_GEN_DIR}")
    target_compile_definitions(rejsrf_game PRIVATE REJSRF_HAS_RECOMP=1)
elseif(REJSRF_DECOMP_SOURCES)
    # Decompiled functions only make sense next to the code they replace.
    set_source_files_properties(${REJSRF_DECOMP_SOURCES} PROPERTIES HEADER_FILE_ONLY ON)
endif()
