/*
 * reaction.c
 */

#include "reaction.h"
#include "app_config.h"
#include "workload_generator.h"
#include "fan.h"
#include "logging.h"

static WorkloadLevel_t cap = WORKLOAD_HIGH;
static uint32_t calm_ticks = 0;
static uint8_t safety = 0;

void Reaction_Init(void)
{
    cap = WORKLOAD_HIGH;
    calm_ticks = 0;
    safety = 0;
    Workload_SetCap(cap);
    Fan_SetPercent(0);
}

static WorkloadLevel_t cap_for_risk(RiskLevel_t r)
{
    switch (r) {
        case RISK_CRITICAL: return WORKLOAD_LOW;
        case RISK_CAUTION:  return WORKLOAD_MEDIUM;
        default:            return WORKLOAD_HIGH;
    }
}

static uint8_t fan_for_cap(WorkloadLevel_t c)
{
    if (c <= WORKLOAD_LOW)    return FAN_PCT_CRITICAL;
    if (c == WORKLOAD_MEDIUM) return FAN_PCT_CAUTION;
    return FAN_PCT_NORMAL;
}

void Reaction_Update(const Prediction_t *pred, float temp_rise_c, float battery_pct, uint8_t enabled)
{
    if (!enabled) {
        cap = WORKLOAD_HIGH;
        calm_ticks = 0;
        safety = 0;
        Workload_SetCap(cap);
        Fan_SetPercent(0);
        return;
    }

    WorkloadLevel_t target = cap_for_risk(pred->temp_risk);

    /* Battery saver: high predicted draw on a nearly empty battery */
    if (pred->power_risk == RISK_CRITICAL && battery_pct < BATTERY_SAVER_PCT && target > WORKLOAD_MEDIUM) {
        target = WORKLOAD_MEDIUM;
    }

    /* Hard safety cut-off on the measured value: the backstop if the model is wrong */
    uint8_t was_safety = safety;
    safety = (temp_rise_c >= Predictor_GetCriticalRise());
    if (safety) {
        target = WORKLOAD_IDLE;
        if (!was_safety) {
            LOG_WARN(TAG_REACT, "Safety cut-off: rise %.1f C >= %.1f C, forcing IDLE",
                     (double)temp_rise_c, (double)Predictor_GetCriticalRise());
        }
    }

    if (target < cap) {
        /* Tighten immediately */
        cap = target;
        calm_ticks = 0;
    } else if (target > cap) {
        /* Loosen one level at a time after a calm period (hysteresis) */
        if (++calm_ticks >= REACT_RELEASE_TICKS) {
            cap = (WorkloadLevel_t)(cap + 1);
            calm_ticks = 0;
        }
    } else {
        calm_ticks = 0;
    }

    Workload_SetCap(cap);
    Fan_SetPercent(safety ? FAN_PCT_CRITICAL : fan_for_cap(cap));
}

uint8_t Reaction_SafetyActive(void)
{
    return safety;
}
