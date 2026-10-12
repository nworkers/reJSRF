# rejsrf_game: everything specific to Jet Set Radio Future.
#
#   jsrf_profile.cpp   identity of supported XBE builds, HLE address bindings
#   jsrf_recomp.cpp    the recompiled entry point, when generated code exists
#   decomp/            hand-decompiled functions that replace lifted ones
#   <gen dir>          lifted C from scripts/recomp/regenerate.py (gitignored)
file(GLOB REJSRF_DECOMP_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/decomp/*.c")

add_library(rejsrf_game STATIC
    jsrf_profile.cpp
    jsrf_recomp.cpp
    ${REJSRF_DECOMP_SOURCES}
)
target_link_libraries(rejsrf_game PUBLIC rexbox_recomp)

if(EXISTS "${REJSRF_RECOMP_GEN_DIR}/recomp_dispatch.c")
    # The generated code must have been lifted against the same ABI header
    # rexbox_recomp compiles with; a different xboxrecomp pin means regenerate.
    file(SHA256 "${REJSRF_RECOMP_GEN_DIR}/recomp_types.h" gen_abi_hash)
    file(SHA256 "${REJSRF_XBOXRECOMP_DIR}/templates/runtime/recomp_types.h" pinned_abi_hash)
    if(NOT gen_abi_hash STREQUAL pinned_abi_hash)
        message(FATAL_ERROR "${REJSRF_RECOMP_GEN_DIR} was generated with a different xboxrecomp; "
                            "run scripts/recomp/regenerate.py")
    endif()

    file(GLOB REJSRF_RECOMP_SOURCES CONFIGURE_DEPENDS "${REJSRF_RECOMP_GEN_DIR}/*.c")
    list(LENGTH REJSRF_RECOMP_SOURCES recomp_count)
    message(STATUS "reJSRF recompiled code: ${recomp_count} files from ${REJSRF_RECOMP_GEN_DIR}")

    add_library(rejsrf_recomp STATIC ${REJSRF_RECOMP_SOURCES})
    target_include_directories(rejsrf_recomp PRIVATE
        "${REJSRF_RECOMP_GEN_DIR}"
        "${REJSRF_XBOXRECOMP_DIR}/src/kernel")  # recomp_icall_feedback.h (MIT)
    if(MSVC)
        # Machine-written code: warnings are noise, and the files are large.
        target_compile_options(rejsrf_recomp PRIVATE /w /bigobj /MP)
    else()
        target_compile_options(rejsrf_recomp PRIVATE -w)
    endif()
    # Diagnostic: every direct call in the generated code reports its target
    # (recomp_call_profile_hit in rexbox_recomp); rejsrf's REXBOX_WATCHDOG dumps it.
    option(REJSRF_RECOMP_CALL_PROFILE "Build the generated code with per-call profiling (slow)" OFF)
    if(REJSRF_RECOMP_CALL_PROFILE)
        target_compile_definitions(rejsrf_recomp PRIVATE RECOMP_CALL_PROFILE)
    endif()
    target_link_libraries(rejsrf_recomp PUBLIC rexbox_recomp)

    # Decompiled sources include the generated declarations.
    target_include_directories(rejsrf_game PRIVATE "${REJSRF_RECOMP_GEN_DIR}")
    target_compile_definitions(rejsrf_game PRIVATE REJSRF_HAS_RECOMP=1)
    target_link_libraries(rejsrf_game PUBLIC rejsrf_recomp)
    # Generated code calls the decompiled sub_X definitions in rejsrf_game, and
    # rejsrf_game calls generated code: a cycle between two static libraries.
    # Declaring both directions makes CMake repeat them on linkers that scan
    # each library once (GNU ld); MSVC resolves the cycle on its own.
    target_link_libraries(rejsrf_recomp PUBLIC rejsrf_game)
else()
    message(STATUS "reJSRF recompiled code: none (run scripts/recomp/regenerate.py)")
    if(REJSRF_DECOMP_SOURCES)
        # Decompiled functions only make sense next to the code they replace.
        set_source_files_properties(${REJSRF_DECOMP_SOURCES} PROPERTIES HEADER_FILE_ONLY ON)
    endif()
endif()
