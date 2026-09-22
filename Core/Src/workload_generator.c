/*
 * workload_generator.c
 *
 *  Created on: Sep 20, 2026
 *      Author: swaro
 */
#include "workload_generator.h"
#include "logging.h"

/* ============================================================
 * Iteration counts per level.
 *
 * These are placeholder values — you WILL need to recalibrate them once
 * you can actually observe real timing/thermal behavior on hardware. The
 * goal is for HIGH to produce a clearly different power/thermal signature
 * than IDLE within one sample-tick period (~1s per the TIM2 config), not
 * to hit any specific millisecond target. Adjust by trial and observation
 * during your exploratory data collection run (see ML_PIPELINE.md).
 *
 * Built at -O0 (per your current CubeIDE Debug config) so these loops
 * won't get optimized away even without the volatile counter below — but
 * the volatile is kept anyway as a safeguard against future optimization
 * level changes (e.g. switching to a Release/-O2 build).
 * ============================================================ */
static const uint32_t WORKLOAD_ITERATIONS[WORKLOAD_LEVEL_COUNT] = {
    [WORKLOAD_IDLE]   = 0,
    [WORKLOAD_LOW]    = 50000,
    [WORKLOAD_MEDIUM] = 200000,
    [WORKLOAD_HIGH]   = 800000,
};

static volatile WorkloadLevel_t current_level = WORKLOAD_IDLE;

/* Prevents the compiler from eliminating the "pointless" loop below even
 * under higher optimization levels — the loop's result is never used for
 * anything except consuming CPU cycles/power, which is the entire point. */
static volatile uint32_t workload_sink = 0;

void Workload_SetLevel(WorkloadLevel_t level)
{
    if (level >= WORKLOAD_LEVEL_COUNT) {
        LOG_WARN(TAG_WKLD, "Requested workload level %d out of range, clamping to HIGH", (int)level);
        level = WORKLOAD_HIGH;
    }

    current_level = level;
    LOG_INFO(TAG_WKLD, "Workload level set to %d", (int)current_level);
}

WorkloadLevel_t Workload_GetLevel(void)
{
    return current_level;
}

void Workload_CycleLevel(void)
{
    WorkloadLevel_t next = (WorkloadLevel_t)((current_level + 1) % WORKLOAD_LEVEL_COUNT);
    Workload_SetLevel(next);
}

void Workload_RunBusyLoop(void)
{
    uint32_t iterations = WORKLOAD_ITERATIONS[current_level];

    for (uint32_t i = 0; i < iterations; i++) {
        /* Deliberately pointless arithmetic — only exists to burn CPU
         * cycles (and therefore power/heat) proportional to `iterations`.
         * Written to `workload_sink` (volatile) so it can't be optimized out. */
        workload_sink = workload_sink + (i ^ 0xA5A5A5A5u);
    }
}


