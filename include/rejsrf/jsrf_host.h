// rejsrf: what the title's hand-written adapters ask of the host. Plain C so
// the decompiled sources (C) can include it; the application fills the hooks
// in, and rejsrf_game links without the HLE (the decomp check tool), where
// each hook's null means its 4:3, no-host answer.
#ifndef REJSRF_JSRF_HOST_H
#define REJSRF_JSRF_HOST_H

#ifdef __cplusplus
extern "C" {
#endif

// How much wider the picture's horizontal view is than the title's 4:3 one:
// the widened back buffer's width over 640, or 1
// (docs/design/20261018-i097-frustum-culling.md). Null: 1.
extern float (*rejsrf_host_horizontal_view_scale)(void);

#ifdef __cplusplus
}
#endif

#endif  // REJSRF_JSRF_HOST_H
