/* Guest locks: host mutual exclusion for code that relied on the Xbox's
 * single core and strict priority scheduling to keep other threads out
 * (docs/design/20261016-i068-adxm-lock.md).
 *
 * Guest threads run in parallel on host threads, so a guest section that
 * only raised its priority is not exclusive here. Title code wraps such a
 * section in rexbox_guest_lock_enter/leave on one named lock.
 *
 * The lock is recursive per thread. A wait that cannot get it at once is
 * noted for the stall report. On the Xbox a lower thread still ran when the
 * holder blocked, so after REXBOX_GUEST_LOCK_TIMEOUT_MS (default 2000, 0 for
 * no limit) the waiter reports once and enters without exclusion rather than
 * deadlock; a leave by a thread that does not own the lock does nothing.
 *
 * Callable from C (decompiled title code). */

#ifndef REXBOX_RUNTIME_GUEST_LOCK_H
#define REXBOX_RUNTIME_GUEST_LOCK_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct RexboxGuestLock
{
    const char* name; /* for reports; static storage */
    void* impl;       /* zero-initialize; made on first use */
} RexboxGuestLock;

void rexbox_guest_lock_enter(RexboxGuestLock* lock);
void rexbox_guest_lock_leave(RexboxGuestLock* lock);

#ifdef __cplusplus
}
#endif

#endif /* REXBOX_RUNTIME_GUEST_LOCK_H */
