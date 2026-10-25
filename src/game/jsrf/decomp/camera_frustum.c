/* Host-concurrency-free game adapter for Hor+ widescreen: the camera's
 * frustum globals (issue #97, docs/design/20261018-i097-frustum-culling.md).
 *
 * Not a decompilation: the lifted body runs unchanged, then a correction
 * follows. JSRF_DECOMP_00153A90 (thiscall, ecx = the camera) turns the field of view
 * into the values the two visibility tests read:
 *
 *   0x264E80, 0x264E88  1 / cos(half horizontal angle)   (plane normal scale)
 *   0x264E84            -tan(half horizontal angle)
 *   0x264E8C             tan(half horizontal angle)
 *   0x264E78, 0x264E90  1 / cos(half vertical angle)
 *   0x264E7C, 0x264E94  +/- tan(half vertical angle)
 *
 * JSRF_DECOMP_00153B70 (a sphere) and JSRF_DECOMP_00153D00 (a point) test a view-space
 * position against x = +/- z * tan (+/- r / cos) and the same for y. Nothing
 * else reads these globals, and the projection matrix is not built from
 * them, so the title keeps its 4:3 projection (the HLE widens the picture)
 * while its culling admits what the widened picture shows. */

#include "recomp_types.h"
#include "rejsrf/jsrf_addresses.h"
#include "rejsrf/jsrf_host.h"

#include <math.h>

extern void JSRF_DECOMP_00153A90_gen(void);

/* 0x00153A90, thiscall: void (camera* this) */
void JSRF_DECOMP_00153A90(void)
{
    JSRF_DECOMP_00153A90_gen();
    const float scale = rejsrf_host_horizontal_view_scale != 0 ? rejsrf_host_horizontal_view_scale() : 1.0f;
    if (scale > 1.0f)
    {
        const float tangent = MEMF(JSRF_FRUSTUM_HTAN_POS) * scale;
        const float secant = sqrtf(1.0f + tangent * tangent);
        MEMF(JSRF_FRUSTUM_HTAN_POS) = tangent;
        MEMF(JSRF_FRUSTUM_HTAN_NEG) = -tangent;
        MEMF(JSRF_FRUSTUM_HSECANT_A) = secant;
        MEMF(JSRF_FRUSTUM_HSECANT_B) = secant;
    }
}
