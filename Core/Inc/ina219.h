/*
 * ina219.h
 *
 *  Created on: Sep 4, 2026
 *      Author: swaro
 */

#ifndef INA219_H
#define INA219_H

#include "main.h"
#include <stdint.h>


/*
 * INA219 default I2C address.
 *
 * A0 = GND
 * A1 = GND
 *
 * Therefore:
 *
 * 7-bit address = 0x40
 */
#define INA219_I2C_ADDRESS    (0x40U)


/*
 * Initialize INA219.
 */
HAL_StatusTypeDef INA219_Init(void);


/*
 * Read bus voltage in millivolts.
 */
HAL_StatusTypeDef INA219_ReadBusVoltage(uint16_t *voltage_mv);


/*
 * Read shunt voltage in microvolts.
 */
HAL_StatusTypeDef INA219_ReadShuntVoltage(int16_t *shunt_uv);


/*
 * Read current in milliamps.
 */
HAL_StatusTypeDef INA219_ReadCurrent(int16_t *current_ma);


/*
 * Read power in milliwatts.
 */
HAL_StatusTypeDef INA219_ReadPower(uint16_t *power_mw);


#endif /* INA219_H */
