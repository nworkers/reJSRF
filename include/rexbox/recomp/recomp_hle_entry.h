/* Entry into the HLE from a generated override of an XDK library function.
 *
 * scripts/recomp/regenerate.py writes, for every identified function of a
 * library chosen for HLE, a definition of the function's lifted symbol that
 * calls rexbox_hle_enter with a static slot. The first call resolves the HLE
 * export of the same name; with none, the run stops and reports the function
 * (docs/design/20261012-i006-xdk-symbol-binding.md). */

#ifndef REXBOX_RECOMP_RECOMP_HLE_ENTRY_H
#define REXBOX_RECOMP_RECOMP_HLE_ENTRY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RexboxHleSlot
{
    uint32_t va;               /* the function's guest address */
    const char* library;       /* XDK library, e.g. "D3D8" */
    const char* name;          /* XDK symbol, e.g. "Direct3D_CreateDevice" */
    void (*lifted)(void);      /* the original body (sub_X_gen) */
    void (*resolved)(void);    /* cached HLE export, filled on first call */
    volatile int32_t state;    /* 0 unresolved, 1 resolved, 2 no export */
} RexboxHleSlot;

void rexbox_hle_enter(RexboxHleSlot* slot);

/* An identified XDK variable (name and guest address), from the same
 * generated file; the table ends with an entry whose name is NULL. A title
 * profile hands these to HleRegistry::BindVariable. */
typedef struct RexboxXdkVariable
{
    const char* library;
    const char* name;
    uint32_t address;
} RexboxXdkVariable;

#ifdef __cplusplus
}
#endif

#endif /* REXBOX_RECOMP_RECOMP_HLE_ENTRY_H */
