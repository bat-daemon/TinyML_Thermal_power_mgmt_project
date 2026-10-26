/*
 * status_led.h
 *
 *  Common-cathode RGB LED: R = TIM3_CH2 (PC7), G = TIM4_CH1 (PB6),
 *  B = TIM14_CH1 (PA7). Timers are configured by CubeMX (period 999).
 */

#ifndef STATUS_LED_H
#define STATUS_LED_H

#include "predictor.h"

void StatusLed_Init(void);
void StatusLed_ShowRisk(RiskLevel_t risk);  /* green / yellow / red */
void StatusLed_ShowCalibrating(void);       /* blue while the baseline is taken */

#endif /* STATUS_LED_H */
