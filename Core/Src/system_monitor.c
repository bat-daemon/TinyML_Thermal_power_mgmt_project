/*
 * system_monitor.c
 *
 *  Created on: Sep 4, 2026
 *      Author: swaro
 */

#include "system_monitor.h"


/**
  * @brief Initialize system monitor.
  *
  * Currently no additional initialization is required.
  */
void SystemMonitor_Init(void)
{
    /*
     * Reserved for future monitoring initialization.
     *
     * Future additions may include:
     *
     * - CPU utilization
     * - memory monitoring
     * - workload monitoring
     * - additional system-state measurements
     */
}


/**
  * @brief Return current HCLK frequency.
  */
uint32_t SystemMonitor_GetCPUFrequency(void)
{
    return HAL_RCC_GetHCLKFreq();
}


/**
  * @brief Return current DWT cycle counter.
  */
uint32_t SystemMonitor_GetCycleCount(void)
{
    return DWT->CYCCNT;
}


/**
  * @brief Calculate CPU workload percentage.
  *
  * CPU Load =
  *
  *        Busy workload cycles
  *       ----------------------- × 100
  *        Total measurement cycles
  *
  */
float SystemMonitor_CalculateCPULoad(
    uint32_t busy_cycles,
    uint32_t total_cycles)
{
    if (total_cycles == 0U)
    {
        return 0.0f;
    }


    return (
        (float)busy_cycles /
        (float)total_cycles
    ) * 100.0f;
}


/**
  * @brief Measure static RAM usage.
  *
  * _sdata = beginning of initialized data
  *
  * _ebss = end of BSS
  *
  * Static RAM =
  *
  *        _ebss - _sdata
  */
uint32_t SystemMonitor_GetStaticRAM(void)
{
    extern uint8_t _sdata;

    extern uint8_t _ebss;


    return (uint32_t)(&_ebss - &_sdata);
}


/**
  * @brief Calculate static RAM utilization.
  *
  * RAM utilization =
  *
  *        Static RAM used
  *       ---------------- × 100
  *        Total SRAM
  */
float SystemMonitor_CalculateRAMPercent(
    uint32_t static_ram_bytes)
{
    return (
        (float)static_ram_bytes /
        (float)TOTAL_SRAM_BYTES
    ) * 100.0f;
}
