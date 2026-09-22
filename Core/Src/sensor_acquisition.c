/*
 * sensor_acquisition.c
 *
 *  Created on: Sep 17, 2026
 *      Author: swaro
 */

#include "sensor_acquisition.h"
#include "logging.h"

/* ============================================================
 * INA219 — I2C address & registers
 * ============================================================ */
#define INA219_I2C_ADDR       (0x40 << 1)   /* 7-bit addr 0x40 (A0=A1=GND), shifted for HAL's 8-bit format */

#define INA219_REG_CONFIG     0x00
#define INA219_REG_SHUNT_V    0x01
#define INA219_REG_BUS_V      0x02
#define INA219_REG_POWER      0x03
#define INA219_REG_CURRENT    0x04
#define INA219_REG_CALIB      0x05

/* Config: 32V bus range, PGA /8 (+-320mV shunt range), 12-bit ADC on both
 * channels, continuous shunt+bus conversion. Standard config for a 0.1 ohm
 * shunt (confirmed via R100 marking) supporting up to ~3.2A measurable current. */
#define INA219_CONFIG_VALUE   0x399F

/* Calibration for Rshunt = 0.1 ohm, max expected current 3.2A:
 * Current_LSB = 0.1 mA/bit
 * Cal = trunc(0.04096 / (Current_LSB_in_A * Rshunt)) = trunc(0.04096 / (0.0001 * 0.1)) = 4096 */
#define INA219_CALIBRATION_VALUE  4096

#define INA219_CURRENT_LSB_MA  0.1f   /* mA per bit, from calibration above */
#define INA219_POWER_LSB_MW    2.0f   /* mW per bit = 20 * Current_LSB */
#define INA219_BUS_V_LSB_MV    4.0f   /* mV per bit, after right-shifting raw register by 3 */

/* ============================================================
 * LM35 — ADC config
 * ============================================================ */
#define ADC_VREF_MV     3300.0f
#define ADC_MAX_COUNTS  4095.0f
#define LM35_MV_PER_C   10.0f
#define ADC_FAIL_TEMP   (-273.15f)   /* impossible reading, signals failure to caller */

/* ============================================================
 * INA219 register I/O
 * ============================================================ */
static bool ina219_write_reg(uint8_t reg, uint16_t value)
{
    uint8_t buf[2] = { (uint8_t)(value >> 8), (uint8_t)(value & 0xFF) }; /* big-endian per datasheet */

    HAL_StatusTypeDef status = HAL_I2C_Mem_Write(&hi2c1, INA219_I2C_ADDR, reg,
                                                  I2C_MEMADD_SIZE_8BIT, buf, 2, 100);
    if (status != HAL_OK) {
        LOG_ERROR(TAG_SENS, "INA219 write reg 0x%02X failed (HAL status %d)", reg, status);
        return false;
    }
    return true;
}

static bool ina219_read_reg(uint8_t reg, uint16_t *value)
{
    uint8_t buf[2];

    HAL_StatusTypeDef status = HAL_I2C_Mem_Read(&hi2c1, INA219_I2C_ADDR, reg,
                                                 I2C_MEMADD_SIZE_8BIT, buf, 2, 100);
    if (status != HAL_OK) {
        LOG_ERROR(TAG_SENS, "INA219 read reg 0x%02X failed (HAL status %d)", reg, status);
        return false;
    }
    *value = (uint16_t)((buf[0] << 8) | buf[1]);
    return true;
}

bool Sensor_Init(void)
{
    bool ok = true;

    ok &= ina219_write_reg(INA219_REG_CONFIG, INA219_CONFIG_VALUE);
    ok &= ina219_write_reg(INA219_REG_CALIB, INA219_CALIBRATION_VALUE);

    if (ok) {
        LOG_INFO(TAG_SENS, "INA219 initialized (config=0x%04X, calib=%d)",
                 INA219_CONFIG_VALUE, INA219_CALIBRATION_VALUE);
    } else {
        LOG_ERROR(TAG_SENS, "INA219 initialization failed — check I2C wiring/address");
    }
    return ok;
}

bool Sensor_ReadPower(float *voltage_v, float *current_ma, float *power_mw)
{
    uint16_t raw_bus, raw_current, raw_power;
    bool ok = true;

    ok &= ina219_read_reg(INA219_REG_BUS_V, &raw_bus);
    ok &= ina219_read_reg(INA219_REG_CURRENT, &raw_current);
    ok &= ina219_read_reg(INA219_REG_POWER, &raw_power);

    if (!ok) {
        *voltage_v = 0.0f;
        *current_ma = 0.0f;
        *power_mw = 0.0f;
        return false;
    }

    /* Bus voltage: bits [15:3] hold the value, LSB = 4mV. Bit 0 = math overflow
     * flag, bit 1 = conversion-ready flag — both ignored here (simple-read path). */
    *voltage_v = ((float)(raw_bus >> 3)) * INA219_BUS_V_LSB_MV / 1000.0f;

    /* Current register is signed 16-bit (two's complement) */
    *current_ma = ((int16_t)raw_current) * INA219_CURRENT_LSB_MA;

    /* Power register is unsigned */
    *power_mw = ((float)raw_power) * INA219_POWER_LSB_MW;

    return true;
}

/* ============================================================
 * LM35 reads
 * ============================================================ */
static float adc_read_channel(uint32_t channel)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = channel;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_15CYCLES;

    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
        LOG_ERROR(TAG_SENS, "ADC channel config failed (channel %lu)", channel);
        return ADC_FAIL_TEMP;
    }

    HAL_ADC_Start(&hadc1);
    if (HAL_ADC_PollForConversion(&hadc1, 10) != HAL_OK) {
        HAL_ADC_Stop(&hadc1);
        LOG_ERROR(TAG_SENS, "ADC conversion timeout (channel %lu)", channel);
        return ADC_FAIL_TEMP;
    }

    uint32_t raw = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);

    return ((float)raw * ADC_VREF_MV / ADC_MAX_COUNTS) / LM35_MV_PER_C;
}

float Sensor_ReadTempPrimary(void)
{
    return adc_read_channel(ADC_CHANNEL_0); /* PA0 */
}

float Sensor_ReadTempAmbient(void)
{
    return adc_read_channel(ADC_CHANNEL_1); /* PA1 */
}

bool Sensor_ReadAll(SensorReading_t *out)
{
    bool power_ok = Sensor_ReadPower(&out->voltage_v, &out->current_ma, &out->power_mw);
    out->temp_primary_c = Sensor_ReadTempPrimary();
    out->temp_ambient_c = Sensor_ReadTempAmbient();
    return power_ok;
}

