/*
 * reaction.h
 *
 *  Stage 1 reaction: on predicted risk, cap the workload and run the fan.
 *  A fixed safety cut-off on the measured temperature rise always applies,
 *  whatever the model says.
 */

#ifndef REACTION_H
#define REACTION_H

#include <stdint.h>
#include "predictor.h"

void    Reaction_Init(void);

/* enabled = 0 in COLLECT/AUTO mode: no cap and fan off, so the recorded
 * data shows the board's natural behaviour (needed for honest labels). */
void    Reaction_Update(const Prediction_t *pred, float temp_rise_c, float battery_pct, uint8_t enabled);

uint8_t Reaction_SafetyActive(void);

#endif /* REACTION_H */
