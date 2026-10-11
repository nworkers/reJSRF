/* Decompiled JSRF functions: small indexed-table accessors.
 *
 * Each definition replaces the lifted function of the same name for direct
 * and indirect calls (scripts/recomp/regenerate.py hands this file to the
 * lifter as --exclude-manual). The `_gen` declarations keep the lifted bodies
 * under that suffix so rejsrf_decomp_check can compare the two. */

#include "recomp_types.h"

extern void sub_000128C0_gen(void);

/* 0x000128C0, thiscall, ret 4: uint32_t (this, int32_t index)
 *
 * Returns this->table_098[index], or 0 for a negative index. The owning
 * class is not identified yet; 742 call sites use it. */
void sub_000128C0(void)
{
    const uint32_t self = g_ecx;
    const int32_t index = (int32_t)MEM32(g_esp + 4);
    g_eax = index < 0 ? 0u : MEM32(self + (uint32_t)index * 4u + 0x98u);
    g_esp += 8;
}
