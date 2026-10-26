/*
 * fan.c
 */

#include "fan.h"
#include "app_config.h"
#include "logging.h"
#include "main.h"

#if FAN_ENABLED

static TIM_HandleTypeDef htim1;
static uint8_t  fan_pct = 0;
static int16_t  override_pct = -1;
static uint32_t kick_end_ms = 0;
static uint8_t  kicking = 0;

static void pwm_write(uint8_t pct)
{
    uint32_t arr = __HAL_TIM_GET_AUTORELOAD(&htim1);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, (arr + 1u) * pct / 100u);
}

void Fan_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_TIM1_CLK_ENABLE();

    /* PB5 = IN1 high, PB4 = IN2 low: fixed direction */
    gpio.Pin = GPIO_PIN_4 | GPIO_PIN_5;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &gpio);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_5, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_RESET);

    /* PA8 = TIM1_CH1 -> L293D EN1 */
    gpio.Pin = GPIO_PIN_8;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Alternate = GPIO_AF1_TIM1;
    HAL_GPIO_Init(GPIOA, &gpio);

    /* TIM1 runs from APB2 timer clock = 2 x PCLK2 = 180 MHz; 1 MHz tick */
    uint32_t tim_clk = 2u * HAL_RCC_GetPCLK2Freq();
    htim1.Instance = TIM1;
    htim1.Init.Prescaler = (tim_clk / 1000000u) - 1u;
    htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim1.Init.Period = (1000000u / FAN_PWM_HZ) - 1u;
    htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim1.Init.RepetitionCounter = 0;
    htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_PWM_Init(&htim1) != HAL_OK) {
        LOG_ERROR(TAG_REACT, "Fan PWM init failed");
        return;
    }

    TIM_OC_InitTypeDef oc = {0};
    oc.OCMode = TIM_OCMODE_PWM1;
    oc.Pulse = 0;
    oc.OCPolarity = TIM_OCPOLARITY_HIGH;
    oc.OCFastMode = TIM_OCFAST_DISABLE;
    oc.OCIdleState = TIM_OCIDLESTATE_RESET;
    if (HAL_TIM_PWM_ConfigChannel(&htim1, &oc, TIM_CHANNEL_1) != HAL_OK) {
        LOG_ERROR(TAG_REACT, "Fan PWM channel config failed");
        return;
    }
    /* Advanced timer: HAL_TIM_PWM_Start also sets MOE so the output is driven */
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    LOG_INFO(TAG_REACT, "Fan ready on PA8 (L293D EN1), %u Hz PWM", (unsigned)FAN_PWM_HZ);
}

static void apply(uint8_t pct)
{
    if (pct > 100u) pct = 100u;
    if (pct > 0u && pct < FAN_MIN_RUN_PCT) pct = FAN_MIN_RUN_PCT;

    if (pct > 0u && fan_pct == 0u) {
        /* Start from standstill at full power, then drop to the target */
        kicking = 1;
        kick_end_ms = HAL_GetTick() + FAN_KICK_MS;
        pwm_write(100u);
    } else if (!kicking) {
        pwm_write(pct);
    }
    fan_pct = pct;
}

void Fan_SetPercent(uint8_t pct)
{
    if (override_pct >= 0) {
        return;   /* manual override wins */
    }
    apply(pct);
}

uint8_t Fan_GetPercent(void)
{
    return fan_pct;
}

void Fan_Service(void)
{
    if (kicking && (int32_t)(HAL_GetTick() - kick_end_ms) >= 0) {
        kicking = 0;
        pwm_write(fan_pct);
    }
}

void Fan_SetOverride(int16_t pct)
{
    if (pct < 0) {
        override_pct = -1;
        LOG_INFO(TAG_REACT, "Fan back to automatic");
        return;
    }
    override_pct = (pct > 100) ? 100 : pct;
    apply((uint8_t)override_pct);
    LOG_INFO(TAG_REACT, "Fan manual override %d%%", (int)override_pct);
}

int16_t Fan_GetOverride(void)
{
    return override_pct;
}

#else  /* FAN_ENABLED == 0 */

void    Fan_Init(void) {}
void    Fan_SetPercent(uint8_t pct) { (void)pct; }
uint8_t Fan_GetPercent(void) { return 0; }
void    Fan_Service(void) {}
void    Fan_SetOverride(int16_t pct) { (void)pct; }
int16_t Fan_GetOverride(void) { return -1; }

#endif
