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

/* Config: 32V bus range, PGA /8 (+-320mV shunt range), 12-bit bus ADC,
 * shunt ADC averaging 128 samples (68 ms), continuous conversion. The MCU
 * current switches between sleep (~19 mA) and run (~44 mA) many times per
 * millisecond, so a single 532 us sample aliases badly; averaging fixes it.
 * 0.1 ohm shunt (confirmed via R100 marking), up to ~3.2A measurable. */
#define INA219_CONFIG_VALUE   0x39FF

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
 * Internal die temperature sensor — factory calibration (RM0390 / DS10693)
 * Raw values were taken at VDDA = 3.3 V, 30 degC and 110 degC.
 * ============================================================ */
#define F4_TS_CAL1_ADDR        ((const uint16_t *)0x1FFF7A2CUL)
#define F4_TS_CAL2_ADDR        ((const uint16_t *)0x1FFF7A2EUL)
#define F4_VREFINT_CAL_ADDR    ((const uint16_t *)0x1FFF7A2AUL)
#define TS_CAL1_TEMP_C      30.0f
#define TS_CAL2_TEMP_C      110.0f
#define DIE_OVERSAMPLE      16u
/* Datasheet typicals, used only if the calibration words look erased */
#define TS_V25_MV           760.0f
#define TS_AVG_SLOPE_MV_C   2.5f

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
/* Averages `samples` conversions of one channel. Returns -1 on failure. */
static float adc_read_raw_avg(uint32_t channel, uint32_t sampling_time, uint32_t samples)
{
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = channel;
    sConfig.Rank = 1;
    sConfig.SamplingTime = sampling_time;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
        LOG_ERROR(TAG_SENS, "ADC channel config failed (channel %lu)", channel);
        return -1.0f;
    }
    uint32_t sum = 0;
    for (uint32_t i = 0; i < samples; i++) {
        HAL_ADC_Start(&hadc1);
        if (HAL_ADC_PollForConversion(&hadc1, 10) != HAL_OK) {
            HAL_ADC_Stop(&hadc1);
            LOG_ERROR(TAG_SENS, "ADC conversion timeout (channel %lu)", channel);
            return -1.0f;
        }
        sum += HAL_ADC_GetValue(&hadc1);
        HAL_ADC_Stop(&hadc1);
    }
    return (float)sum / (float)samples;
}

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

float Sensor_ReadPotPercent(void)
{
    float raw = adc_read_raw_avg(ADC_CHANNEL_0, ADC_SAMPLETIME_84CYCLES, 8u); /* PA0 */
    if (raw < 0.0f) {
        return 0.0f;
    }
    return raw * 100.0f / ADC_MAX_COUNTS;
}

float Sensor_ReadTempAmbient(void)
{
    return adc_read_channel(ADC_CHANNEL_1); /* PA1 */
}

float Sensor_ReadTempDie(void)
{
    /* The sensor needs >= 10 us sampling; 480 cycles at 22.5 MHz ADC clock = 21 us.
     * HAL_ADC_ConfigChannel sets TSVREFE in ADC->CCR for these two channels. */
    float vref_raw = adc_read_raw_avg(ADC_CHANNEL_VREFINT, ADC_SAMPLETIME_480CYCLES, DIE_OVERSAMPLE);
    float ts_raw   = adc_read_raw_avg(ADC_CHANNEL_TEMPSENSOR, ADC_SAMPLETIME_480CYCLES, DIE_OVERSAMPLE);
    if (vref_raw <= 0.0f || ts_raw <= 0.0f) {
        return ADC_FAIL_TEMP;
    }

    uint16_t cal1 = *F4_TS_CAL1_ADDR;
    uint16_t cal2 = *F4_TS_CAL2_ADDR;
    uint16_t vref_cal = *F4_VREFINT_CAL_ADDR;

    if (cal1 == 0 || cal1 == 0xFFFF || cal2 <= cal1 || vref_cal == 0 || vref_cal == 0xFFFF) {
        /* Fallback: datasheet typicals, assuming VDDA = 3.3 V */
        float mv = ts_raw * ADC_VREF_MV / ADC_MAX_COUNTS;
        return (mv - TS_V25_MV) / TS_AVG_SLOPE_MV_C + 25.0f;
    }

    /* Rescale the reading to what it would be at VDDA = 3.3 V (the calibration condition) */
    float ts_at_cal_vdda = ts_raw * ((float)vref_cal / vref_raw);
    return (TS_CAL2_TEMP_C - TS_CAL1_TEMP_C) / (float)(cal2 - cal1)
           * (ts_at_cal_vdda - (float)cal1) + TS_CAL1_TEMP_C;
}

bool Sensor_ReadAll(SensorReading_t *out)
{
    bool power_ok = Sensor_ReadPower(&out->voltage_v, &out->current_ma, &out->power_mw);
    out->temp_die_c = Sensor_ReadTempDie();
    out->temp_ambient_c = Sensor_ReadTempAmbient();
    return power_ok;
}

