/* Host-concurrency adapter for the CRI ADXM lock callbacks (issue #68,
 * docs/design/20261016-i068-adxm-lock.md).
 *
 * Not a decompilation: the lifted bodies run unchanged, wrapped in a host
 * lock. ADXM_SetupThrd (0x0013B330) installs these two as the SVM lock and
 * unlock callbacks. They keep one global nesting count and one saved
 * priority, and get their exclusion from raising the caller to
 * TIME_CRITICAL on the Xbox's single core. Guest threads run in parallel on
 * the host, so without a real lock two threads mix the count and the saved
 * priority, and one is left at TIME_CRITICAL holding nothing (0x001437B0
 * then loops for ever). */

#include "recomp_types.h"
#include "rexbox/runtime/guest_lock.h"

extern void sub_0013B0A0_gen(void);
extern void sub_0013B0E0_gen(void);

static RexboxGuestLock g_adxm_lock = {"CRI ADXM lock", 0};

/* 0x0013B0A0, cdecl: void (void* unused)
 *
 * Lock: at count 0, saves the caller's priority, raises it to TIME_CRITICAL
 * and resumes the ADXM safe thread; then count + 1. */
void sub_0013B0A0(void)
{
    rexbox_guest_lock_enter(&g_adxm_lock);
    sub_0013B0A0_gen();
}

/* 0x0013B0E0, cdecl: void (void* unused)
 *
 * Unlock: count - 1; at 0, suspends the safe thread and restores the saved
 * priority. */
void sub_0013B0E0(void)
{
    sub_0013B0E0_gen();
    rexbox_guest_lock_leave(&g_adxm_lock);
}
