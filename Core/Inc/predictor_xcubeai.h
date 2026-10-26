/*
 * predictor_xcubeai.h
 *
 *  Thin wrapper around the X-CUBE-AI generated network (see predictor_xcubeai.c).
 */

#ifndef PREDICTOR_XCUBEAI_H
#define PREDICTOR_XCUBEAI_H

#define XAI_N_IN   7
#define XAI_N_OUT  3

/* Plain C network on standardised inputs, giving logits; used to self-check */
typedef void (*XAI_RefFn)(const float x_scaled[XAI_N_IN], float logits_t[XAI_N_OUT], float logits_p[XAI_N_OUT]);

/* Returns 0 when the network is created and agrees with the reference */
int XAI_Init(XAI_RefFn reference);

/* Runs one inference on standardised inputs; outputs are softmax probabilities */
int XAI_Run(const float x_scaled[XAI_N_IN], float out_t[XAI_N_OUT], float out_p[XAI_N_OUT]);

#endif /* PREDICTOR_XCUBEAI_H */
