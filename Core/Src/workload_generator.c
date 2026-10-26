/*
 * workload_generator.c
 *
 *  Created on: Sep 20, 2026
 *      Author: swaro
 *
 *  Reworked for the closed loop: each level is now a duty cycle of the
 *  1 s sample slot instead of a fixed iteration count, so the power levels
 *  are repeatable without calibration. IDLE spends the whole slot in
 *  __WFI() sleep (done by the caller), which is what makes IDLE and HIGH
 *  clearly different in power.
 */

#include "workload_generator.h"
#include "app_config.h"
#include "logging.h"
#include "main.h"
#include <string.h>

static const uint32_t WORKLOAD_DUTY[WORKLOAD_LEVEL_COUNT] = {
    [WORKLOAD_IDLE]   = WORKLOAD_DUTY_IDLE,
    [WORKLOAD_LOW]    = WORKLOAD_DUTY_LOW,
    [WORKLOAD_MEDIUM] = WORKLOAD_DUTY_MEDIUM,
    [WORKLOAD_HIGH]   = WORKLOAD_DUTY_HIGH,
};

static volatile WorkloadLevel_t current_level = WORKLOAD_IDLE;
static volatile WorkloadLevel_t cap_level = WORKLOAD_HIGH;

/* Working set for the busy loop: FPU math plus memory traffic, so the
 * core, FPU, flash accelerator and SRAM bus are all switching. */
#define WORK_BUF_WORDS 1024u
static uint32_t work_src[WORK_BUF_WORDS];
static uint32_t work_dst[WORK_BUF_WORDS];
static volatile float workload_sink_f = 0.0f;

/* AUTO schedule state */
static uint8_t  auto_enabled = 0;
static uint32_t auto_rng = 1u;
static uint32_t auto_segment_end_ms = 0;

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

void Workload_SetCap(WorkloadLevel_t cap)
{
    if (cap >= WORKLOAD_LEVEL_COUNT) {
        cap = WORKLOAD_HIGH;
    }
    if (cap != cap_level) {
        LOG_INFO(TAG_WKLD, "Workload cap set to %d", (int)cap);
    }
    cap_level = cap;
}

WorkloadLevel_t Workload_GetCap(void)
{
    return cap_level;
}

WorkloadLevel_t Workload_GetEffectiveLevel(void)
{
    return (current_level < cap_level) ? current_level : cap_level;
}

uint32_t Workload_GetDutyPercent(void)
{
    return WORKLOAD_DUTY[Workload_GetEffectiveLevel()];
}

/* One short chunk of work (~tens of microseconds) */
static void busy_chunk(void)
{
    float acc = workload_sink_f;
    for (uint32_t i = 0; i < 64u; i++) {
        acc = acc * 1.000001f + 0.5f / ((float)i + 1.0f);
    }
    memcpy(work_dst, work_src, sizeof(work_src));
    work_src[(uint32_t)acc & (WORK_BUF_WORDS - 1u)] ^= 0xA5A5A5A5u;
    workload_sink_f = acc;
}

void Workload_RunSlice(uint32_t slot_start_ms, uint32_t slot_len_ms, volatile uint8_t *abort_flag)
{
    uint32_t busy_ms = slot_len_ms * Workload_GetDutyPercent() / 100u;
    while ((HAL_GetTick() - slot_start_ms) < busy_ms) {
        if (abort_flag != NULL && *abort_flag) {
            return;
        }
        busy_chunk();
    }
}

void Workload_RunBusyLoop(void)
{
    /* Kept for compatibility with older call sites: one full-slot slice. */
    Workload_RunSlice(HAL_GetTick(), SAMPLE_PERIOD_MS, NULL);
}

/* ============================================================
 * AUTO schedule
 * ============================================================ */
static uint32_t rng_next(void)
{
    /* xorshift32 */
    auto_rng ^= auto_rng << 13;
    auto_rng ^= auto_rng >> 17;
    auto_rng ^= auto_rng << 5;
    return auto_rng;
}

static uint32_t rng_range(uint32_t lo, uint32_t hi)
{
    return lo + (rng_next() % (hi - lo + 1u));
}

void Workload_AutoEnable(uint8_t enable, uint32_t seed)
{
    auto_enabled = enable;
    auto_rng = (seed != 0u) ? seed : 0x1234567u;
    auto_segment_end_ms = 0;
    LOG_INFO(TAG_WKLD, "AUTO schedule %s", enable ? "on" : "off");
}

void Workload_AutoStep(uint32_t now_ms)
{
    if (!auto_enabled || (int32_t)(now_ms - auto_segment_end_ms) < 0) {
        return;
    }

    /* Mix of segment types so the dataset holds every pattern the model
     * must recognise: long heat-ups, cool-downs, short bursts, mid holds. */
    uint32_t r = rng_range(0, 99);
    WorkloadLevel_t level;
    uint32_t seconds;
    if (r < 30) {            /* long heat-up */
        level = WORKLOAD_HIGH;
        seconds = rng_range(60, 180);
    } else if (r < 55) {     /* cool-down */
        level = WORKLOAD_IDLE;
        seconds = rng_range(60, 150);
    } else if (r < 70) {     /* short burst: should NOT become Critical */
        level = WORKLOAD_HIGH;
        seconds = rng_range(4, 12);
    } else if (r < 85) {
        level = WORKLOAD_MEDIUM;
        seconds = rng_range(30, 120);
    } else {
        level = WORKLOAD_LOW;
        seconds = rng_range(20, 90);
    }
    Workload_SetLevel(level);
    auto_segment_end_ms = now_ms + seconds * 1000u;
}
