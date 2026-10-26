/*
 * feature_extraction.h
 *
 *  Sliding-window features fed to the model. The exact same maths lives in
 *  scripts/train_model.py (compute_features) — change both together.
 */

#ifndef FEATURE_EXTRACTION_H
#define FEATURE_EXTRACTION_H

#include <stdint.h>

/* Feature vector order (index -> meaning) */
#define FEAT_TEMP_RISE      0   /* degC above idle baseline (smoothed) */
#define FEAT_TEMP_SLOPE10   1   /* degC/s over the window */
#define FEAT_TEMP_SLOPE3    2   /* degC/s over the last 3 s */
#define FEAT_POWER_NOW      3   /* mW */
#define FEAT_POWER_AVG10    4   /* mW, window mean */
#define FEAT_POWER_SLOPE10  5   /* mW/s over the window */
#define FEAT_WORKLOAD       6   /* effective workload level 0..3 */
#define FEAT_COUNT          7

void Features_Reset(void);

/* Push one sample (one per tick) and compute the feature vector. */
void Features_Update(float temp_rise_c, float power_mw, uint8_t workload_eff, float out[FEAT_COUNT]);

#endif /* FEATURE_EXTRACTION_H */
