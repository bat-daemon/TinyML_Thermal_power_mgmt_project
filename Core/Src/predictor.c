/*
 * predictor.c
 */

#include "predictor.h"
#include "model_weights.h"
#include "main.h"

#if MODEL_TRAINED && (MODEL_N_FEAT != FEAT_COUNT)
#error "model_weights.h was trained with a different feature count than feature_extraction.h"
#endif

static float caution_rise = MODEL_CAUTION_RISE_C;
static float critical_rise = MODEL_CRITICAL_RISE_C;

void Predictor_Init(void)
{
    /* DWT cycle counter, used to time inference */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

void Predictor_SetLimits(float caution_rise_c, float critical_rise_c)
{
    if (caution_rise_c > 0.0f && critical_rise_c > caution_rise_c) {
        caution_rise = caution_rise_c;
        critical_rise = critical_rise_c;
    }
}

float Predictor_GetCautionRise(void)  { return caution_rise; }
float Predictor_GetCriticalRise(void) { return critical_rise; }
uint32_t Predictor_GetHorizonS(void)  { return MODEL_HORIZON_S; }

const char *Risk_Name(RiskLevel_t r)
{
    switch (r) {
        case RISK_NORMAL:   return "NORMAL";
        case RISK_CAUTION:  return "CAUTION";
        case RISK_CRITICAL: return "CRITICAL";
        default:            return "?";
    }
}

static RiskLevel_t classify(float value, float caution, float critical)
{
    if (value >= critical) return RISK_CRITICAL;
    if (value >= caution)  return RISK_CAUTION;
    return RISK_NORMAL;
}

/* Baseline: extrapolate the current trend H seconds ahead and threshold it.
 * Mirrors rule_predict() in scripts/train_model.py. */
static void rule_predict(const float f[FEAT_COUNT], RiskLevel_t *temp, RiskLevel_t *power)
{
    float h = (float)MODEL_HORIZON_S;
    float t_slope = (f[FEAT_TEMP_SLOPE10] > 0.0f) ? f[FEAT_TEMP_SLOPE10] : 0.0f;
    float rise_future = f[FEAT_TEMP_RISE] + t_slope * h;
    float p_future = f[FEAT_POWER_NOW] + f[FEAT_POWER_SLOPE10] * h;
    *temp = classify(rise_future, caution_rise, critical_rise);
    *power = classify(p_future, MODEL_CAUTION_POWER_MW, MODEL_CRITICAL_POWER_MW);
}

#if MODEL_TRAINED
static uint8_t argmax3(const float v[MODEL_N_CLASSES])
{
    uint8_t best = 0;
    for (uint8_t i = 1; i < MODEL_N_CLASSES; i++) {
        if (v[i] > v[best]) best = i;
    }
    return best;
}

/* Dense(H1, relu) -> Dense(H2, relu) -> two Dense(3) heads. Softmax is not
 * needed: argmax of the logits gives the same class. */
static void mlp_predict(const float f[FEAT_COUNT], RiskLevel_t *temp, RiskLevel_t *power)
{
    float x[MODEL_N_FEAT];
    float h1[MODEL_HIDDEN1];
    float h2[MODEL_HIDDEN2];
    float out_t[MODEL_N_CLASSES];
    float out_p[MODEL_N_CLASSES];

    for (int i = 0; i < MODEL_N_FEAT; i++) {
        x[i] = (f[i] - MODEL_FEAT_MEAN[i]) / MODEL_FEAT_STD[i];
    }
    for (int j = 0; j < MODEL_HIDDEN1; j++) {
        float acc = MODEL_B1[j];
        for (int i = 0; i < MODEL_N_FEAT; i++) acc += MODEL_W1[j][i] * x[i];
        h1[j] = (acc > 0.0f) ? acc : 0.0f;
    }
    for (int j = 0; j < MODEL_HIDDEN2; j++) {
        float acc = MODEL_B2[j];
        for (int i = 0; i < MODEL_HIDDEN1; i++) acc += MODEL_W2[j][i] * h1[i];
        h2[j] = (acc > 0.0f) ? acc : 0.0f;
    }
    for (int k = 0; k < MODEL_N_CLASSES; k++) {
        float at = MODEL_BT[k];
        float ap = MODEL_BP[k];
        for (int i = 0; i < MODEL_HIDDEN2; i++) {
            at += MODEL_WT[k][i] * h2[i];
            ap += MODEL_WP[k][i] * h2[i];
        }
        out_t[k] = at;
        out_p[k] = ap;
    }
    *temp = (RiskLevel_t)argmax3(out_t);
    *power = (RiskLevel_t)argmax3(out_p);
}
#endif

void Predictor_Run(const float features[FEAT_COUNT], Prediction_t *out)
{
    rule_predict(features, &out->rule_temp_risk, &out->rule_power_risk);
#if MODEL_TRAINED
    uint32_t c0 = DWT->CYCCNT;
    mlp_predict(features, &out->temp_risk, &out->power_risk);
    out->inference_us = (DWT->CYCCNT - c0) / (SystemCoreClock / 1000000u);
    out->model_used = 1;
#else
    out->temp_risk = out->rule_temp_risk;
    out->power_risk = out->rule_power_risk;
    out->inference_us = 0;
    out->model_used = 0;
#endif
}
