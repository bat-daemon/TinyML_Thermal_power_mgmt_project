/*
 * status_led.c
 */

#include "status_led.h"
#include "app_config.h"
#include "tim.h"

static uint8_t held = 0;

/* Duty 0..1000 (timer period is 999) */
static void set_rgb(uint32_t r, uint32_t g, uint32_t b)
{
#if LED_COMMON_ANODE
    r = 1000u - r;
    g = 1000u - g;
    b = 1000u - b;
#endif
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_2, r);
    __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, g);
    __HAL_TIM_SET_COMPARE(&htim14, TIM_CHANNEL_1, b);
}

void StatusLed_Init(void)
{
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_2);
    HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim14, TIM_CHANNEL_1);
    /* Self-test so wiring problems show at boot: red, green, blue */
    set_rgb(1000, 0, 0);  HAL_Delay(300);
    set_rgb(0, 1000, 0);  HAL_Delay(300);
    set_rgb(0, 0, 1000);  HAL_Delay(300);
    set_rgb(0, 0, 0);
}

void StatusLed_ShowRisk(RiskLevel_t risk)
{
    if (held) return;
    switch (risk) {
        case RISK_NORMAL:   set_rgb(0, 600, 0);    break;
        case RISK_CAUTION:  set_rgb(1000, 350, 0); break;
        case RISK_CRITICAL: set_rgb(1000, 0, 0);   break;
        default:            set_rgb(0, 0, 0);      break;
    }
}

void StatusLed_Test(uint8_t r_pct, uint8_t g_pct, uint8_t b_pct)
{
    if (r_pct > 100u) r_pct = 100u;
    if (g_pct > 100u) g_pct = 100u;
    if (b_pct > 100u) b_pct = 100u;
    set_rgb(r_pct * 10u, g_pct * 10u, b_pct * 10u);
}

void StatusLed_Hold(uint8_t hold)
{
    held = hold;
}

void StatusLed_ShowCalibrating(void)
{
    set_rgb(0, 0, 600);
}
