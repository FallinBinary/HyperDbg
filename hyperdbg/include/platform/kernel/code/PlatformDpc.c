/**
 * @file PlatformDpc.c
 * @author Sina Karvandi (sina@hyperdbg.org)
 * @brief Implementation of cross platform APIs for Deferred Procedure Call (DPC) management
 * @details
 * @version 0.19
 * @date 2026-05-09
 *
 * @copyright This project is released under the GNU Public License v3.
 *
 */
#include "pch.h"

#if defined(__linux__)
#    include "../header/PlatformDpc.h"

/**
 * @brief BH-workqueue trampoline for the Linux KDPC backing
 *
 * @details A Linux work callback receives only the work_struct pointer, whereas
 * the Windows DPC contract hands the deferred routine four arguments. Recover
 * the enclosing KDPC and replay the Windows-style (Dpc, Context, Arg1, Arg2)
 * call. SKELETON — compile-clean wiring, not yet runtime-tested.
 *
 * @param Work The work_struct embedded in the owning KDPC
 * @return void
 */
static void
PlatformDpcWorkTrampoline(struct work_struct * Work)
{
    KDPC * Dpc = container_of(Work, KDPC, Work);

    if (Dpc->DeferredRoutine != NULL)
    {
        Dpc->DeferredRoutine(Dpc,
                             Dpc->DeferredContext,
                             Dpc->SystemArgument1,
                             Dpc->SystemArgument2);
    }
}
#endif // defined(__linux__)

/**
 * @brief Initialize a DPC object
 *
 * @param Dpc Pointer to the KDPC structure to initialize
 * @param DeferredRoutine The deferred procedure to be called
 * @param DeferredContext Optional context passed to the deferred routine
 * @return VOID
 */
VOID
PlatformDpcInitialize(PRKDPC Dpc, PKDEFERRED_ROUTINE DeferredRoutine, PVOID DeferredContext)
{
#if defined(_WIN32) || defined(_WIN64)

    KeInitializeDpc(Dpc, DeferredRoutine, DeferredContext);

#elif defined(__linux__)

    //
    // Stash the Windows-style routine + context so the tasklet trampoline can
    // replay the 4-argument call. System arguments are supplied at queue time.
    //
    Dpc->DeferredRoutine = DeferredRoutine;
    Dpc->DeferredContext = DeferredContext;
    Dpc->SystemArgument1 = NULL;
    Dpc->SystemArgument2 = NULL;

    INIT_WORK(&Dpc->Work, PlatformDpcWorkTrampoline);

#else

#    error "Unsupported platform"

#endif
}

/**
 * @brief Insert a DPC into the system DPC queue for execution
 *
 * @param Dpc Pointer to the initialized KDPC structure
 * @param SystemArgument1 First system-defined argument passed to the deferred routine
 * @param SystemArgument2 Second system-defined argument passed to the deferred routine
 * @return BOOLEAN TRUE if the DPC was successfully queued, FALSE if it was already in the queue
 */
BOOLEAN
PlatformDpcInsertQueueDpc(PRKDPC Dpc, PVOID SystemArgument1, PVOID SystemArgument2)
{
#if defined(_WIN32) || defined(_WIN64)

    return KeInsertQueueDpc(Dpc, SystemArgument1, SystemArgument2);

#elif defined(__linux__)

    //
    // Supply the system arguments for this run, then hand the work item to the
    // bottom-half (BH) workqueue, which runs it in softirq context — the closest
    // match to a DPC at DISPATCH_LEVEL. queue_work() is safe from atomic/
    // interrupt context, matching KeInsertQueueDpc.
    //
    // queue_work() returns false if the item was already pending, mirroring
    // KeInsertQueueDpc's "FALSE if already queued" contract, so return it directly.
    //
    Dpc->SystemArgument1 = SystemArgument1;
    Dpc->SystemArgument2 = SystemArgument2;

    return queue_work(system_bh_wq, &Dpc->Work) ? TRUE : FALSE;

#else

#    error "Unsupported platform"

#endif
}
