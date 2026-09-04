/*
 * system_monitor.h
 *
 *  Created on: Sep 4, 2026
 *      Author: swaro
 */

#ifndef SYSTEM_MONITOR_H
#define SYSTEM_MONITOR_H

#include "main.h"
#include <stdint.h>


/*
 * Total SRAM available on STM32F446RE.
 */
#define TOTAL_SRAM_BYTES    (128U * 1024U)


/*
 * Initialize system monitoring functionality.
 */
void SystemMonitor_Init(void);


/*
 * Get current CPU frequency in Hz.
 */
uint32_t SystemMonitor_GetCPUFrequency(void);


/*
 * Get current DWT cycle counter value.
 */
uint32_t SystemMonitor_GetCycleCount(void);


/*
 * Calculate CPU workload percentage.
 */
float SystemMonitor_CalculateCPULoad(
    uint32_t busy_cycles,
    uint32_t total_cycles
);


/*
 * Get static RAM usage in bytes.
 */
uint32_t SystemMonitor_GetStaticRAM(void);


/*
 * Calculate static RAM utilization percentage.
 */
float SystemMonitor_CalculateRAMPercent(
    uint32_t static_ram_bytes
);


#endif /* SYSTEM_MONITOR_H */
