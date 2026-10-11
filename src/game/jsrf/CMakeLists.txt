# rejsrf_game: everything specific to Jet Set Radio Future.
#
#   jsrf_profile.cpp  identity of supported XBE builds, HLE address bindings
#   recomp/           lifted C generated from the user's XBE (gitignored)
#   decomp/           hand-decompiled functions that replace lifted ones
#
# recomp/ and decomp/ join this target when the recompilation pipeline lands
# (see docs/design/20261012-i001-project-bootstrap.md).
add_library(rejsrf_game STATIC
    jsrf_profile.cpp
)
target_link_libraries(rejsrf_game PUBLIC rexbox_runtime)
