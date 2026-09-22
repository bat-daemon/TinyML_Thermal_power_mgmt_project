/*
 * workload_generator.h
 *
 *  Created on: Sep 20, 2026
 *      Author: swaro
 */

#ifndef INC_WORKLOAD_GENERATOR_H_
#define INC_WORKLOAD_GENERATOR_H_

#include <stdint.h>

/* ============================================================
 * Workload intensity levels
 * ============================================================
 * These map to increasing busy-loop iteration counts (see .c file).
 * The numeric value IS the "workload level" fed to the ML model as a
 * known input (per ML_PIPELINE.md) and logged in the DATA, CSV row
 * (per module_logging.md's workload_level field) — so don't reorder
 * these without updating anything downstream that assumes the enum
 * values are meaningful, ordered integers.
 */
typedef enum {
    WORKLOAD_IDLE   = 0,
    WORKLOAD_LOW    = 1,
    WORKLOAD_MEDIUM = 2,
    WORKLOAD_HIGH   = 3,
    WORKLOAD_LEVEL_COUNT   /* not a real level — used for bounds checking */
} WorkloadLevel_t;

/* ============================================================
 * Public API (matches module_workload_generator.md's planned signatures)
 * ============================================================ */

/* Sets the current workload intensity. Clamps to a valid level if out of range. */
void Workload_SetLevel(WorkloadLevel_t level);

/* Returns the current workload level — this is the "known input" value
 * Feature Extraction/ML Inference read, and what gets logged per DATA, row. */
WorkloadLevel_t Workload_GetLevel(void);

/* Cycles to the next level (wraps back to IDLE after HIGH). Intended to be
 * called from the B1 button's EXTI callback for manual demo control. */
void Workload_CycleLevel(void);

/* Runs one burst of the busy-loop at the current intensity. Call this once
 * per superloop tick (i.e. once per TIM2 sample-tick interrupt), NOT in a
 * free-running fashion — it's a bounded, single burst per call so it
 * doesn't starve the rest of the pipeline (sensor reads, logging, etc.)
 * within a given tick period. */
void Workload_RunBusyLoop(void);

#endif /* INC_WORKLOAD_GENERATOR_H_ */
