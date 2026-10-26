/*
 * feature_extraction.c
 */

#include "feature_extraction.h"
#include "app_config.h"

static float rise_hist[FEATURE_WINDOW];
static float power_hist[FEATURE_WINDOW];
static uint32_t count = 0;   /* samples pushed since reset */

void Features_Reset(void)
{
    count = 0;
}

/* Value pushed `back` samples ago (0 = newest). Caller guarantees back < count. */
static float hist_get(const float *hist, uint32_t back)
{
    uint32_t idx = (count - 1u - back) % FEATURE_WINDOW;
    return hist[idx];
}

/* (newest - value `span` samples ago) / span, with span clipped to what exists */
static float slope(const float *hist, uint32_t span)
{
    uint32_t avail = (count < FEATURE_WINDOW) ? count : FEATURE_WINDOW;
    if (span > avail - 1u) {
        span = avail - 1u;
    }
    if (span == 0u) {
        return 0.0f;
    }
    return (hist_get(hist, 0) - hist_get(hist, span)) / (float)span;
}

void Features_Update(float temp_rise_c, float power_mw, uint8_t workload_eff, float out[FEAT_COUNT])
{
    rise_hist[count % FEATURE_WINDOW] = temp_rise_c;
    power_hist[count % FEATURE_WINDOW] = power_mw;
    count++;

    uint32_t avail = (count < FEATURE_WINDOW) ? count : FEATURE_WINDOW;
    float p_sum = 0.0f;
    for (uint32_t i = 0; i < avail; i++) {
        p_sum += hist_get(power_hist, i);
    }

    out[FEAT_TEMP_RISE]     = temp_rise_c;
    out[FEAT_TEMP_SLOPE10]  = slope(rise_hist, FEATURE_WINDOW - 1u);
    out[FEAT_TEMP_SLOPE3]   = slope(rise_hist, 3u);
    out[FEAT_POWER_NOW]     = power_mw;
    out[FEAT_POWER_AVG10]   = p_sum / (float)avail;
    out[FEAT_POWER_SLOPE10] = slope(power_hist, FEATURE_WINDOW - 1u);
    out[FEAT_WORKLOAD]      = (float)workload_eff;
}
