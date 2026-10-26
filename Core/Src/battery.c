/*
 * battery.c
 */

#include "battery.h"
#include "app_config.h"
#include "logging.h"

static float capacity_mah;
static float voltage_v;
static float energy_total_mwh;
static float energy_used_mwh;
static float avg_draw_mw;    /* exponential average of battery-side power */
static uint8_t avg_valid;

void Battery_Init(void)
{
    Battery_Set(BATTERY_DEFAULT_MAH, BATTERY_DEFAULT_V);
}

void Battery_Set(float cap_mah, float volt)
{
    if (cap_mah <= 0.0f || volt <= 0.0f) {
        LOG_WARN(TAG_REACT, "Ignoring invalid battery %.0f mAh %.2f V", (double)cap_mah, (double)volt);
        return;
    }
    capacity_mah = cap_mah;
    voltage_v = volt;
    energy_total_mwh = cap_mah * volt;
    energy_used_mwh = 0.0f;
    avg_valid = 0;
    LOG_INFO(TAG_REACT, "Battery set: %.0f mAh @ %.2f V = %.2f Wh",
             (double)cap_mah, (double)volt, (double)(energy_total_mwh / 1000.0f));
}

void Battery_Update(float mcu_power_mw, float dt_s)
{
    if (mcu_power_mw < 0.0f) mcu_power_mw = 0.0f;
    float draw_mw = mcu_power_mw / BATTERY_REG_EFFICIENCY;

    energy_used_mwh += draw_mw * dt_s / 3600.0f;
    if (energy_used_mwh > energy_total_mwh) energy_used_mwh = energy_total_mwh;

    if (!avg_valid) {
        avg_draw_mw = draw_mw;
        avg_valid = 1;
    } else {
        float alpha = dt_s / BATTERY_AVG_TAU_S;
        if (alpha > 1.0f) alpha = 1.0f;
        avg_draw_mw += alpha * (draw_mw - avg_draw_mw);
    }
}

float Battery_GetPercent(void)
{
    return 100.0f * (energy_total_mwh - energy_used_mwh) / energy_total_mwh;
}

float Battery_GetHoursLeft(void)
{
    if (!avg_valid || avg_draw_mw <= 0.0f) return 0.0f;
    return (energy_total_mwh - energy_used_mwh) / avg_draw_mw;
}

float Battery_GetCapacityMah(void) { return capacity_mah; }
float Battery_GetVoltage(void)     { return voltage_v; }
