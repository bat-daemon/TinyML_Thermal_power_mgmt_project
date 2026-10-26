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

/* Caps the level actually run (Stage 1 reaction). The requested level is
 * kept, so when the cap is lifted the workload returns to what was asked. */
void Workload_SetCap(WorkloadLevel_t cap);
WorkloadLevel_t Workload_GetCap(void);

/* min(requested level, cap) — what actually runs and what the model sees. */
WorkloadLevel_t Workload_GetEffectiveLevel(void);

/* Duty cycle of the effective level, in percent of each sample slot. */
uint32_t Workload_GetDutyPercent(void);

/* Burns CPU for the busy part of the current sample slot, then returns.
 * slot_start_ms is HAL_GetTick() at the start of the slot. Returns early
 * if *abort_flag becomes non-zero (e.g. the next sample tick or a UART
 * command arrived). The caller sleeps (__WFI) for the rest of the slot. */
void Workload_RunSlice(uint32_t slot_start_ms, uint32_t slot_len_ms, volatile uint8_t *abort_flag);

/* AUTO data-collection schedule: when enabled, Workload_AutoStep() picks
 * random levels for random durations (ramps, bursts, long holds, cooldowns)
 * so a dataset can be recorded unattended. Call once per sample tick. */
void Workload_AutoEnable(uint8_t enable, uint32_t seed);
void Workload_AutoStep(uint32_t now_ms);

#endif /* INC_WORKLOAD_GENERATOR_H_ */
