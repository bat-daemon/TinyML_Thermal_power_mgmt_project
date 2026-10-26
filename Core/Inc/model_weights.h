/*
 * model_weights.h
 *
 *  PLACEHOLDER — overwritten by scripts/train_model.py after training.
 *  With MODEL_TRAINED 0 the predictor falls back to the slope-threshold
 *  rule, so the firmware builds and runs before any model exists.
 */

#ifndef MODEL_WEIGHTS_H
#define MODEL_WEIGHTS_H

#define MODEL_TRAINED            0
#define MODEL_N_FEAT             7
#define MODEL_HORIZON_S          15
#define MODEL_CAUTION_RISE_C     6.0f
#define MODEL_CRITICAL_RISE_C    9.0f
#define MODEL_CAUTION_POWER_MW   170.0f
#define MODEL_CRITICAL_POWER_MW  200.0f

#endif /* MODEL_WEIGHTS_H */
