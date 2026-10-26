/*
 * predictor.h
 *
 *  ML inference (small dense network, weights in model_weights.h) plus the
 *  slope-threshold rule baseline. Both run every tick so they can be
 *  compared live and in the logged data.
 */

#ifndef PREDICTOR_H
#define PREDICTOR_H

#include <stdint.h>
#include "feature_extraction.h"

typedef enum {
    RISK_NORMAL   = 0,
    RISK_CAUTION  = 1,
    RISK_CRITICAL = 2,
} RiskLevel_t;

typedef struct {
    RiskLevel_t temp_risk;       /* model output, or rule output if no model */
    RiskLevel_t power_risk;
    RiskLevel_t rule_temp_risk;  /* baseline, always computed */
    RiskLevel_t rule_power_risk;
    uint8_t     model_used;      /* 1 if a trained model produced temp/power risk */
    uint32_t    inference_us;    /* time spent in the network, for the report */
} Prediction_t;

void Predictor_Init(void);
void Predictor_Run(const float features[FEAT_COUNT], Prediction_t *out);

/* Rise thresholds (degC above baseline) used by the rule and the safety cut-off */
void  Predictor_SetLimits(float caution_rise_c, float critical_rise_c);
float Predictor_GetCautionRise(void);
float Predictor_GetCriticalRise(void);
uint32_t Predictor_GetHorizonS(void);

const char *Risk_Name(RiskLevel_t r);

#endif /* PREDICTOR_H */
