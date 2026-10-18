/* Host-concurrency adapter for the CRI ADXM lock callbacks (issue #68,
 * docs/design/20261016-i068-adxm-lock.md; issue #91,
 * docs/design/20261018-i091-adxm-lock-priority.md).
 *
 * Not a decompilation: the lifted bodies run unchanged, wrapped in a host
 * lock. ADXM_SetupThrd (0x0013B330) installs these two as the SVM lock and
 * unlock callbacks. They keep one global nesting count and one saved
 * priority, and get their exclusion from raising the caller to
 * TIME_CRITICAL on the Xbox's single core. Guest threads run in parallel on
 * the host, so without a real lock two threads mix the count and the saved
 * priority, and one is left at TIME_CRITICAL holding nothing (0x001437B0
 * then loops for ever).
 *
 * The host lock is put down while its holder waits, so two threads can still
 * be inside together and leave out of order. Each thread therefore also
 * returns to its own priority when its last unlock is done (issue #91), and
 * the "release all while TIME_CRITICAL" loop 0x001437B0 releases this
 * thread's depth instead (issue #100). */

#include "recomp_types.h"
#include "rexbox/runtime/guest_lock.h"

#include <stdio.h>

extern void sub_0013B0A0_gen(void);
extern void sub_0013B0E0_gen(void);
extern void sub_00147CC0(void); /* XAPI SetThreadPriority, stdcall (HANDLE, int) */
extern void sub_00147D12(void); /* XAPI GetThreadPriority, stdcall (HANDLE) */
extern void sub_00141B80(void); /* SVM_Unlock: the unlock callback through its pointer */

static RexboxGuestLock g_adxm_lock = {"CRI ADXM lock", 0};

/* This thread's lock depth, and its priority at its outermost lock. */
static RECOMP_TLS uint32_t t_depth;
static RECOMP_TLS int32_t t_priority;
static volatile long g_restores; /* reported at the first */
static volatile long g_lowered;  /* reported at the first */

#define CURRENT_THREAD 0xFFFFFFFEu /* GetCurrentThread() */
#define THREAD_PRIORITY_TIME_CRITICAL 15

static int32_t CurrentThreadPriority(void)
{
    PUSH32(g_esp, CURRENT_THREAD);
    PUSH32(g_esp, 0); /* return address; ret 4 pops it with the argument */
    sub_00147D12();
    return (int32_t)g_eax;
}

static void SetCurrentThreadPriority(int32_t priority)
{
    PUSH32(g_esp, (uint32_t)priority);
    PUSH32(g_esp, CURRENT_THREAD);
    PUSH32(g_esp, 0);
    sub_00147CC0();
}

/* 0x0013B0A0, cdecl: void (void* unused)
 *
 * Lock: at count 0, saves the caller's priority, raises it to TIME_CRITICAL
 * and resumes the ADXM safe thread; then count + 1. */
void sub_0013B0A0(void)
{
    rexbox_guest_lock_enter(&g_adxm_lock);
    if (t_depth++ == 0)
    {
        t_priority = CurrentThreadPriority();
    }
    sub_0013B0A0_gen();
}

/* 0x0013B0E0, cdecl: void (void* unused)
 *
 * Unlock: count - 1; at 0, suspends the safe thread and restores the saved
 * priority. That saved priority may be another thread's when two left out
 * of order, so a thread done with the lock takes back its own. */
void sub_0013B0E0(void)
{
    sub_0013B0E0_gen();
    if (t_depth != 0 && --t_depth == 0)
    {
        const int32_t priority = CurrentThreadPriority();
        if (priority != t_priority)
        {
            SetCurrentThreadPriority(t_priority);
            if (g_restores++ == 0)
            {
                fprintf(stderr, "[adxm lock] a thread left the lock at priority %d; its own %d restored (issue #91)\n",
                        (int)priority, (int)t_priority);
            }
        }
    }
    rexbox_guest_lock_leave(&g_adxm_lock);
}

/* 0x001437B0, cdecl: int (void)
 *
 * The original releases the SVM lock while the caller is TIME_CRITICAL
 * (`while (GetThreadPriority() == 15) SVM_Unlock(); return n;`) and
 * 0x001437D0 takes it back n times. "TIME_CRITICAL means holding the lock"
 * is single-core reasoning; on the host a thread can end up TIME_CRITICAL
 * with nothing to release, and the loop never ends (issue #100). This
 * thread's own depth is what it holds, so release that, and lower a
 * priority left behind. */
void sub_001437B0(void)
{
    uint32_t released = 0;
    while (t_depth != 0)
    {
        PUSH32(g_esp, 0); /* return address */
        sub_00141B80();
        ++released;
    }
    const int32_t priority = CurrentThreadPriority();
    if (priority == THREAD_PRIORITY_TIME_CRITICAL)
    {
        SetCurrentThreadPriority(t_priority);
        if (g_lowered++ == 0)
        {
            fprintf(stderr, "[adxm lock] a thread was TIME_CRITICAL holding no lock; its own %d restored (issue #100)\n",
                    (int)t_priority);
        }
    }
    g_eax = released;
    g_esp += 4; /* ret: a lifted callee pops its return address */
}
