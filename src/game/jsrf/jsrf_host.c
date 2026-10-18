/* The host hooks of include/rejsrf/jsrf_host.h: defined here, outside
 * decomp/, because the decompiled sources are compiled only next to the
 * generated code, while the application sets the hooks in every build. */
#include "rejsrf/jsrf_host.h"

float (*rejsrf_host_horizontal_view_scale)(void) = 0;
