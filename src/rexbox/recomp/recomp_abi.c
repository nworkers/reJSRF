/* Definitions of the state recompiled code declares extern.
 *
 * Compiled against the pinned xboxrecomp template header (the same file the
 * lifter copies next to the generated code), so every definition matches its
 * declaration exactly. Functions the contract declares are implemented in
 * recomp_bridge.cpp. */

/* The MMX register type is only declared for generated code, which also maps
 * bare register names (eax, ...) to the globals. Take the declarations and
 * drop the name mapping, which would rewrite RexboxRecompGprs's fields. */
#define RECOMP_GENERATED_CODE
#include "recomp_types.h"
#undef eax
#undef ecx
#undef edx
#undef esp
#undef ebx
#undef esi
#undef edi

#include "rexbox/recomp/recomp_abi.h"

#include <string.h>

/* Memory and code range. */
ptrdiff_t g_xbox_mem_offset;
uint32_t g_xbox_code_lo;
uint32_t g_xbox_code_hi;
int g_force_return;

/* Per-thread guest CPU state. */
RECOMP_TLS uint32_t g_eax, g_ecx, g_edx, g_esp;
RECOMP_TLS uint32_t g_ebx, g_esi, g_edi;
RECOMP_TLS uint32_t g_ebp;
RECOMP_TLS double g_fp_stack[8];
RECOMP_TLS int g_fp_top;
RECOMP_TLS uint16_t g_fp_control_word;
RECOMP_TLS int g_fp_cmp;
RECOMP_TLS uint16_t g_fp_cc;
RECOMP_TLS uint32_t g_mxcsr;
RECOMP_TLS uint32_t g_fs_base;
RECOMP_TLS uint32_t g_seh_ebp;
RECOMP_TLS int g_df;
RECOMP_TLS RecompXmm g_xmm0, g_xmm1, g_xmm2, g_xmm3;
RECOMP_TLS RecompXmm g_xmm4, g_xmm5, g_xmm6, g_xmm7;
RECOMP_TLS RecompMmx g_mm0, g_mm1, g_mm2, g_mm3;
RECOMP_TLS RecompMmx g_mm4, g_mm5, g_mm6, g_mm7;

/* Indirect-call trace ring, shared by all threads like the lifter expects. */
volatile uint32_t g_icall_trace[ICALL_TRACE_SIZE];
volatile uint32_t g_icall_trace_idx;
volatile uint64_t g_icall_count;

/* x87 FINIT control word: all exceptions masked, 64-bit precision, round to
 * nearest. */
#define REXBOX_X87_DEFAULT_CONTROL_WORD 0x037F

/* MXCSR reset value: all SSE exceptions masked, round to nearest. */
#define REXBOX_SSE_DEFAULT_MXCSR 0x1F80u

void rexbox_recomp_set_memory(ptrdiff_t offset, uint32_t code_lo, uint32_t code_hi)
{
    g_xbox_mem_offset = offset;
    g_xbox_code_lo = code_lo;
    g_xbox_code_hi = code_hi;
}

void rexbox_recomp_reset_thread(uint32_t esp, uint32_t fs_base)
{
    g_eax = g_ecx = g_edx = g_ebx = g_esi = g_edi = g_ebp = 0;
    g_esp = esp;
    memset(g_fp_stack, 0, sizeof(g_fp_stack));
    g_fp_top = 0;
    g_fp_control_word = REXBOX_X87_DEFAULT_CONTROL_WORD;
    g_fp_cmp = 0;
    g_fp_cc = 0;
    g_mxcsr = REXBOX_SSE_DEFAULT_MXCSR;
    g_fs_base = fs_base;
    g_seh_ebp = 0;
    g_df = 0;
}

void rexbox_recomp_get_gprs(RexboxRecompGprs* out)
{
    out->eax = g_eax;
    out->ecx = g_ecx;
    out->edx = g_edx;
    out->ebx = g_ebx;
    out->esp = g_esp;
    out->ebp = g_ebp;
    out->esi = g_esi;
    out->edi = g_edi;
}

void rexbox_recomp_set_gprs(const RexboxRecompGprs* in)
{
    g_eax = in->eax;
    g_ecx = in->ecx;
    g_edx = in->edx;
    g_ebx = in->ebx;
    g_esp = in->esp;
    g_ebp = in->ebp;
    g_esi = in->esi;
    g_edi = in->edi;
}

static uint32_t* gpr_slot(int index)
{
    switch (index)
    {
    case 0: return &g_eax;
    case 1: return &g_ecx;
    case 2: return &g_edx;
    case 3: return &g_ebx;
    case 4: return &g_esp;
    case 5: return &g_ebp;
    case 6: return &g_esi;
    default: return &g_edi;
    }
}

uint32_t rexbox_recomp_get_gpr(int index)
{
    return *gpr_slot(index);
}

void rexbox_recomp_set_gpr(int index, uint32_t value)
{
    *gpr_slot(index) = value;
}

uint32_t rexbox_recomp_get_fs_base(void)
{
    return g_fs_base;
}

uint32_t rexbox_recomp_last_icall(void)
{
    if (g_icall_count == 0)
    {
        return 0;
    }
    return g_icall_trace[(g_icall_trace_idx - 1) & (ICALL_TRACE_SIZE - 1)];
}
