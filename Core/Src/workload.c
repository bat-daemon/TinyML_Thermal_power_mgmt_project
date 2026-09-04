/*
 * workload.c
 *
 *  Created on: Sep 4, 2026
 *      Author: swaro
 */

#include "workload.h"


/*
 * Synthetic workload variable.
 *
 * volatile prevents the compiler from completely
 * optimizing away the arithmetic operations.
 */
static volatile uint32_t workload_dummy = 0U;


/**
  * @brief Initialize ARM Cortex-M4 DWT cycle counter.
  */
void Workload_Init(void)
{
    /*
     * Enable trace functionality.
     */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;


    /*
     * Reset cycle counter.
     */
    DWT->CYCCNT = 0U;


    /*
     * Enable cycle counter.
     */
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}


/**
  * @brief Execute synthetic CPU workload.
  *
  * @param required_cycles
  *        Number of CPU cycles for which the
  *        workload should execute.
  */
void Workload_Run(uint32_t required_cycles)
{
    uint32_t start_cycle;


    /*
     * Record starting cycle count.
     */
    start_cycle = DWT->CYCCNT;


    /*
     * Execute arithmetic operations until the
     * requested number of CPU cycles has elapsed.
     */
    while ((uint32_t)(DWT->CYCCNT - start_cycle) < required_cycles)
    {
        workload_dummy += 3U;

        workload_dummy ^= (workload_dummy << 1);

        workload_dummy += 7U;

        workload_dummy ^= (workload_dummy >> 2);
    }
}


/**
  * @brief Return workload percentage.
  */
uint32_t Workload_GetPercent(WorkloadLevel_t level)
{
    switch (level)
    {
        case WORKLOAD_LOW:

            return 20U;


        case WORKLOAD_MEDIUM:

            return 50U;


        case WORKLOAD_HIGH:

            return 80U;


        default:

            return 20U;
    }
}


/**
  * @brief Return workload name.
  */
const char *Workload_GetName(WorkloadLevel_t level)
{
    switch (level)
    {
        case WORKLOAD_LOW:

            return "LOW";


        case WORKLOAD_MEDIUM:

            return "MEDIUM";


        case WORKLOAD_HIGH:

            return "HIGH";


        default:

            return "UNKNOWN";
    }
}
