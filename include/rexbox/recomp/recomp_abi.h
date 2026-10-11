/* Recompiled-code ABI: the runtime side, callable from C and C++.
 *
 * Generated code (xboxrecomp's recomp_types.h contract) keeps the guest CPU
 * state in per-thread globals. This header is the only way the rest of the
 * runtime reads or sets that state, so nothing outside rexbox_recomp depends
 * on the lifter's variable names. */

#ifndef REXBOX_RECOMP_RECOMP_ABI_H
#define REXBOX_RECOMP_RECOMP_ABI_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Guest general-purpose registers, in x86 encoding order. */
typedef struct RexboxRecompGprs
{
    uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi;
} RexboxRecompGprs;

/* host = guest + offset for every guest memory access; code_lo/code_hi bound
 * the executable sections an indirect call may target. */
void rexbox_recomp_set_memory(ptrdiff_t offset, uint32_t code_lo, uint32_t code_hi);

/* Resets the calling thread's guest state: all registers zero except esp and
 * the FS base, x87 control word at its power-on value, empty x87 stack. */
void rexbox_recomp_reset_thread(uint32_t esp, uint32_t fs_base);

void rexbox_recomp_get_gprs(RexboxRecompGprs* out);
void rexbox_recomp_set_gprs(const RexboxRecompGprs* in);

/* One register by x86 encoding index (0 = eax ... 7 = edi). */
uint32_t rexbox_recomp_get_gpr(int index);
void rexbox_recomp_set_gpr(int index, uint32_t value);
uint32_t rexbox_recomp_get_fs_base(void);

/* The most recent indirect-call target on this thread, 0 if none. */
uint32_t rexbox_recomp_last_icall(void);

#ifdef __cplusplus
}
#endif

#endif /* REXBOX_RECOMP_RECOMP_ABI_H */
