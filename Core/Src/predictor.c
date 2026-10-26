/*
 * predictor.c
 */

#include "predictor.h"
#include "model_weights.h"
#include "main.h"
#include "app_config.h"
#if INFERENCE_ENGINE_XCUBEAI
#include "predictor_xcubeai.h"
#endif

#if MODEL_TRAINED && (MODEL_N_FEAT != FEAT_COUNT)
#error "model_weights.h was trained with a different feature count than feature_extraction.h"
#endif

#if INFERENCE_ENGINE_XCUBEAI && !MODEL_TRAINED
#error "INFERENCE_ENGINE_XCUBEAI needs a trained model_weights.h (the plain C network is the self-check reference)"
#endif

#if MODEL_TRAINED
static void mlp_logits(const float x[MODEL_N_FEAT], float out_t[MODEL_N_CLASSES], float out_p[MODEL_N_CLASSES]);
#endif

static float caution_rise = MODEL_CAUTION_RISE_C;
static uint8_t xai_ok = 0;            /* X-CUBE-AI initialised and self-checked */
static uint32_t engine_mismatches = 0; /* ticks where X-CUBE-AI and plain C chose different classes */
static float critical_rise = MODEL_CRITICAL_RISE_C;

void Predictor_Init(void)
{
    /* DWT cycle counter, used to time inference */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
#if INFERENCE_ENGINE_XCUBEAI
    xai_ok = (XAI_Init(mlp_logits) == 0);
#endif
}

uint8_t Predictor_EngineIsXcubeAi(void) { return xai_ok; }
uint32_t Predictor_GetEngineMismatches(void) { return engine_mismatches; }

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

static void scale_features(const float f[FEAT_COUNT], float x[MODEL_N_FEAT])
{
    for (int i = 0; i < MODEL_N_FEAT; i++) {
        x[i] = (f[i] - MODEL_FEAT_MEAN[i]) / MODEL_FEAT_STD[i];
    }
}

/* Dense(H1, relu) -> Dense(H2, relu) -> two Dense(3) heads, on standardised
 * inputs. Returns logits: softmax is not needed for the class, since argmax
 * of the logits gives the same answer. */
static void mlp_logits(const float x[MODEL_N_FEAT], float out_t[MODEL_N_CLASSES], float out_p[MODEL_N_CLASSES])
{
    float h1[MODEL_HIDDEN1];
    float h2[MODEL_HIDDEN2];

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
}
#endif

void Predictor_Run(const float features[FEAT_COUNT], Prediction_t *out)
{
    rule_predict(features, &out->rule_temp_risk, &out->rule_power_risk);
    out->inference_c_us = 0;
    out->inference_xai_us = 0;
#if MODEL_TRAINED
    float x[MODEL_N_FEAT], lt[MODEL_N_CLASSES], lp[MODEL_N_CLASSES];
    uint32_t us_div = SystemCoreClock / 1000000u;

    /* Scaling is part of the measured time for both engines */
    uint32_t c0 = DWT->CYCCNT;
    scale_features(features, x);
    mlp_logits(x, lt, lp);
    out->inference_c_us = (DWT->CYCCNT - c0) / us_div;
    out->temp_risk = (RiskLevel_t)argmax3(lt);
    out->power_risk = (RiskLevel_t)argmax3(lp);
    out->model_used = 1;
    out->engine_xcubeai = 0;
    out->inference_us = out->inference_c_us;

#if INFERENCE_ENGINE_XCUBEAI
    if (xai_ok) {
        float pt[MODEL_N_CLASSES], pp[MODEL_N_CLASSES], xs[MODEL_N_FEAT];
        c0 = DWT->CYCCNT;
        scale_features(features, xs);
        int rc = XAI_Run(xs, pt, pp);
        out->inference_xai_us = (DWT->CYCCNT - c0) / us_div;
        if (rc == 0) {
            RiskLevel_t xt = (RiskLevel_t)argmax3(pt);
            RiskLevel_t xp = (RiskLevel_t)argmax3(pp);
            if (xt != out->temp_risk || xp != out->power_risk) {
                engine_mismatches++;
            }
            out->temp_risk = xt;
            out->power_risk = xp;
            out->engine_xcubeai = 1;
            out->inference_us = out->inference_xai_us;
        }
    }
#endif
#else
    out->temp_risk = out->rule_temp_risk;
    out->power_risk = out->rule_power_risk;
    out->inference_us = 0;
    out->model_used = 0;
    out->engine_xcubeai = 0;
#endif
}
