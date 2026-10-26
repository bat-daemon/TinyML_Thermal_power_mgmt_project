/*
 * sensor_acquisition.h
 *
 *  Created on: Sep 17, 2026
 *      Author: swaro
 */

#ifndef INC_SENSOR_ACQUISITION_H_
#define INC_SENSOR_ACQUISITION_H_

#include "i2c.h"   /* CubeMX-generated — provides hi2c1 handle */
#include "adc.h"   /* CubeMX-generated — provides hadc1 handle */
#include <stdint.h>
#include <stdbool.h>

/* ============================================================
 * Unified sensor reading — matches the DATA, CSV field order
 * in module_logging.md (minus timestamp/workload, added by the caller)
 * ============================================================ */
typedef struct {
    float power_mw;
    float voltage_v;
    float current_ma;
    float temp_die_c;       /* STM32 internal die sensor (factory calibrated) — the trend being predicted */
    float temp_ambient_c;   /* LM35 in open air on PA1 — logged only, not fed to model */
} SensorReading_t;

/* ============================================================
 * Public API (matches module_sensor_acquisition.md's planned signatures)
 * ============================================================ */

/* Initializes and calibrates the INA219 over I2C1. Call once at startup,
 * after MX_I2C1_Init() and MX_ADC1_Init(). Returns true on success. */
bool Sensor_Init(void);

/* Reads INA219 bus voltage/current/power in one call (all three come from
 * the same set of INA219 registers, so one I2C transaction sequence covers them). */
bool Sensor_ReadPower(float *voltage_v, float *current_ma, float *power_mw);

/* Reads the primary (chip-taped) LM35 via ADC1 channel IN0 (PA0).
 * No longer used by the pipeline: the internal die sensor replaced it. */
float Sensor_ReadTempPrimary(void);

/* Reads the STM32F446 internal die temperature sensor, oversampled and
 * corrected for VDDA using the factory VREFINT/TS calibration values. */
float Sensor_ReadTempDie(void);

/* Reads the workload potentiometer wiper on PA0 (A0), 0-100 % of 3V3. */
float Sensor_ReadPotPercent(void);

/* Reads the ambient LM35 via ADC1 channel IN1 (PA1). */
float Sensor_ReadTempAmbient(void);

/* Convenience: populates a full SensorReading_t in one call. Returns false
 * if the INA219 read failed (temp reads cannot fail the same way, since ADC
 * polling has no NACK-style failure mode). */
bool Sensor_ReadAll(SensorReading_t *out);


#endif /* INC_SENSOR_ACQUISITION_H_ */
