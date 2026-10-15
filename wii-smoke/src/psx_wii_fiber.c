/*
 * psx_wii_fiber.c — Wii cooperative stackful fibers backed by libogc LWP.
 *
 * PSXRecomp models BIOS ChangeThread using stackful fibers. libogc has no
 * ucontext API, so each emulated BIOS thread receives one LWP stack but only
 * one emulated fiber is allowed to run at a time. A semaphore hand-off parks
 * the old LWP at the exact switch call and resumes it later with its C stack
 * intact. These are not concurrent guest CPUs: each switch wakes one fiber and
 * blocks the caller before returning.
 */
#include "psx_fiber.h"

#include <gccore.h>
#include <ogc/lwp.h>
#include <ogc/semaphore.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

typedef struct PsxWiiFiber {
    lwp_t thread;
    sem_t run_sem;
    psx_fiber_entry entry;
    void *arg;
    jmp_buf entry_env;
    int is_main;
    volatile int destroyed;
    int entry_started;
} PsxWiiFiber;

static PsxWiiFiber s_main_fiber;
static int s_main_initialized;
static PsxWiiFiber * volatile s_current_fiber;

static void *psx_wii_fiber_thread(void *opaque)
{
    PsxWiiFiber *fiber = (PsxWiiFiber *)opaque;

    for (;;) {
        if (LWP_SemWait(fiber->run_sem) < 0) break;
        if (fiber->destroyed) break;

        s_current_fiber = fiber;
        if (setjmp(fiber->entry_env) != 0) {
            /* Destroy wakes a suspended fiber only so it can unwind to here. */
            break;
        }

        fiber->entry_started = 1;
        fiber->entry(fiber->arg);

        /* The PSXRecomp BIOS-thread entry normally parks at its return-fiber
         * switch and is later destroyed by the scheduler. A genuinely returned
         * entry is considered finished; do not run it again. */
        break;
    }

    return NULL;
}

psx_fiber_t psx_fiber_convert_thread(void)
{
    if (!s_main_initialized) {
        memset(&s_main_fiber, 0, sizeof(s_main_fiber));
        s_main_fiber.thread = LWP_GetSelf();
        s_main_fiber.is_main = 1;
        if (LWP_SemInit(&s_main_fiber.run_sem, 0, 1) < 0)
            return NULL;
        s_main_initialized = 1;
        s_current_fiber = &s_main_fiber;
    }
    return &s_main_fiber;
}

psx_fiber_t psx_fiber_current(void)
{
    if (!s_main_initialized && !psx_fiber_convert_thread())
        return NULL;
    return (psx_fiber_t)s_current_fiber;
}

psx_fiber_t psx_fiber_create(size_t stack_size, psx_fiber_entry entry, void *arg)
{
    PsxWiiFiber *fiber;
    u32 bytes;

    if (!entry) return NULL;
    if (!s_main_initialized && !psx_fiber_convert_thread()) return NULL;

    fiber = (PsxWiiFiber *)calloc(1, sizeof(*fiber));
    if (!fiber) return NULL;

    fiber->entry = entry;
    fiber->arg = arg;
    fiber->thread = LWP_THREAD_NULL;
    bytes = stack_size > 0xFFFFFFFFu ? 0xFFFFFFFFu : (u32)stack_size;
    if (bytes < 256u * 1024u) bytes = 256u * 1024u;

    if (LWP_SemInit(&fiber->run_sem, 0, 1) < 0) {
        free(fiber);
        return NULL;
    }

    if (LWP_CreateThread(&fiber->thread, psx_wii_fiber_thread, fiber,
                         NULL, bytes, (u8)(LWP_PRIO_HIGHEST - 8)) < 0) {
        LWP_SemDestroy(fiber->run_sem);
        free(fiber);
        return NULL;
    }

    /* The new LWP blocks at run_sem before it can execute guest instructions. */
    return fiber;
}

void psx_fiber_switch(psx_fiber_t target_handle)
{
    PsxWiiFiber *target = (PsxWiiFiber *)target_handle;
    PsxWiiFiber *from = (PsxWiiFiber *)s_current_fiber;

    if (!target || !from || target == from) return;

    s_current_fiber = target;
    LWP_SemPost(target->run_sem);
    LWP_SemWait(from->run_sem);

    if (from->destroyed && !from->is_main)
        longjmp(from->entry_env, 1);

    s_current_fiber = from;
}

void psx_fiber_destroy(psx_fiber_t handle)
{
    PsxWiiFiber *fiber = (PsxWiiFiber *)handle;
    void *thread_result = NULL;

    if (!fiber || fiber->is_main || fiber == s_current_fiber) return;

    /* The fiber is parked in its last psx_fiber_switch. Wake that call, have it
     * longjmp to the worker boundary, then join before freeing semaphore/state. */
    fiber->destroyed = 1;
    LWP_SemPost(fiber->run_sem);
    if (fiber->thread != LWP_THREAD_NULL)
        LWP_JoinThread(fiber->thread, &thread_result);
    LWP_SemDestroy(fiber->run_sem);
    free(fiber);
}
