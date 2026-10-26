/*
 * battery.h
 *
 *  "What-if" battery estimate. No battery is connected: the user enters a
 *  capacity and voltage ("BAT 2000 3.7"), and the MCU power measured by the
 *  INA219 is integrated against it.
 */

#ifndef BATTERY_H
#define BATTERY_H

#include <stdint.h>

void  Battery_Init(void);
void  Battery_Set(float capacity_mah, float voltage_v);   /* also refills to 100 % */
void  Battery_Update(float mcu_power_mw, float dt_s);
float Battery_GetPercent(void);
float Battery_GetHoursLeft(void);    /* at the recent average draw */
float Battery_GetCapacityMah(void);
float Battery_GetVoltage(void);

#endif /* BATTERY_H */
