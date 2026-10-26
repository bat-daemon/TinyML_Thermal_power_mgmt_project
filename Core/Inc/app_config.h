/*
 * app_config.h
 *
 *  Central place for every tunable used by the closed-loop pipeline.
 *  Change values here, not inside the modules.
 */

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* ============================================================
 * Timing
 * ============================================================ */
#define SAMPLE_PERIOD_MS        1000u  /* TIM2 fires at 1 Hz (PSC 8999, ARR 9999 @ 90 MHz) */
#define FEATURE_WINDOW          10u    /* samples (= seconds) per sliding window */
#define TEMP_SMOOTH_N           4u     /* moving average applied to the die temperature */
#define BASELINE_SAMPLES        10u    /* idle samples averaged at boot for the baseline */
#define POWER_SAMPLE_MS         50u    /* INA219 is read this often and averaged per tick */
#define WORKLOAD_SLICE_MAX_MS   20u    /* busy loop returns to the main loop at least this often */

/* ============================================================
 * Default thresholds (rise above the idle baseline, in degC).
 * Used until a trained model_weights.h provides its own values,
 * and can be changed at runtime with the "LIM" UART command.
 * ============================================================ */
#define DEFAULT_CAUTION_RISE_C   6.0f
#define DEFAULT_CRITICAL_RISE_C  9.0f
#define DEFAULT_HORIZON_S        15u

/* ============================================================
 * Workload duty cycle per level, in percent of each 1 s slot.
 * IDLE sleeps with __WFI() for the whole slot.
 * ============================================================ */
#define WORKLOAD_DUTY_IDLE      0u
#define WORKLOAD_DUTY_LOW       30u
#define WORKLOAD_DUTY_MEDIUM    65u
#define WORKLOAD_DUTY_HIGH      100u

/* ============================================================
 * Stage 1 reaction
 * ============================================================ */
#define REACT_RELEASE_TICKS     10u    /* calm ticks needed before the cap is raised one level */
#define FAN_PCT_NORMAL          0u
#define FAN_PCT_CAUTION         60u
#define FAN_PCT_CRITICAL        100u
#define BATTERY_SAVER_PCT       20.0f  /* below this, Critical power risk caps the workload */

/* ============================================================
 * Fan: L293D, channel 1
 *   EN1 (pin 1)  <- PA8 (Arduino D7), TIM1_CH1 PWM
 *   IN1 (pin 2)  <- PB5 (Arduino D4)
 *   IN2 (pin 7)  <- PB4 (Arduino D5)
 *   VCC1 (pin 16) = 5 V, VCC2 (pin 8) = 12 V, all GNDs common with the Nucleo
 *   Fan between OUT1 (pin 3) and OUT2 (pin 6)
 * Set FAN_ENABLED to 0 to compile the fan out entirely.
 * ============================================================ */
#define FAN_ENABLED             1
#define FAN_PWM_HZ              5000u
#define FAN_MIN_RUN_PCT         35u    /* below this most 12 V fans stall, so it is raised */
#define FAN_KICK_MS             400u   /* full power for this long when starting from 0 */

/* ============================================================
 * OLED (SSD1306 128x64, I2C1 shared with the INA219)
 *   VCC -> 3V3, GND -> GND, SCL -> PB8 (D15), SDA -> PB9 (D14)
 * Detected at boot; the firmware runs the same without it.
 * Set OLED_ENABLED to 0 to compile the display out entirely.
 * ============================================================ */
#define OLED_ENABLED            1
#define OLED_I2C_ADDR           0x3Cu  /* some modules are 0x3D */

/* ============================================================
 * Inference engine
 *   0 = plain C exported by train_model.py (model_weights.h)
 *   1 = X-CUBE-AI runtime (needs the X-CUBE-AI pack enabled in the .ioc
 *       with a network named "network" from model_out/model.tflite;
 *       see docs/XCUBEAI.md). The plain C network still runs next to it
 *       every tick, so both inference times and any disagreement are logged.
 * ============================================================ */
#ifndef INFERENCE_ENGINE_XCUBEAI
#define INFERENCE_ENGINE_XCUBEAI  0
#endif

/* ============================================================
 * Potentiometer workload dial (optional)
 *   Pot ends -> 3V3 and GND, wiper -> PA0 (Arduino A0)
 *   "WL POT" turns it on; "WL 0-3" or the B1 button go back to manual.
 *   The dial is split into the same 4 levels the model was trained on.
 * ============================================================ */
#define POT_ON_AT_BOOT          0      /* 1 = dial controls the workload from reset */
#define POT_HYSTERESIS_PCT      4.0f   /* stops the level flickering at a boundary */

/* ============================================================
 * RGB status LED
 * ============================================================ */
#define LED_COMMON_ANODE        0      /* set to 1 if the RGB LED's common leg goes to 3V3 */

/* ============================================================
 * Battery estimate (no real battery: the user enters one with "BAT")
 * ============================================================ */
#define BATTERY_DEFAULT_MAH     2000.0f
#define BATTERY_DEFAULT_V       3.7f
#define BATTERY_REG_EFFICIENCY  0.85f  /* INA219 sees the 3.3 V rail, not the battery */
#define BATTERY_AVG_TAU_S       30.0f  /* averaging time for "time left" */

#endif /* APP_CONFIG_H */
