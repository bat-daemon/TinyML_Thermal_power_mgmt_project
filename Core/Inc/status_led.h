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
void StatusLed_Test(uint8_t r_pct, uint8_t g_pct, uint8_t b_pct);  /* "LED r g b" command */
void StatusLed_Hold(uint8_t hold);          /* 1 = keep the test colour, ignore risk updates */

#endif /* STATUS_LED_H */
