/*
 * workload.h
 *
 *  Created on: Sep 4, 2026
 *      Author: swaro
 */

#ifndef WORKLOAD_H
#define WORKLOAD_H

#include "main.h"
#include <stdint.h>


/*
 * Controlled workload levels.
 */
typedef enum
{
    WORKLOAD_LOW = 0,
    WORKLOAD_MEDIUM,
    WORKLOAD_HIGH

} WorkloadLevel_t;


/*
 * Initialize DWT cycle counter.
 */
void Workload_Init(void);


/*
 * Execute CPU workload for the requested number of cycles.
 */
void Workload_Run(uint32_t required_cycles);


/*
 * Return workload percentage corresponding to
 * the selected workload level.
 */
uint32_t Workload_GetPercent(WorkloadLevel_t level);


/*
 * Return workload name as a string.
 */
const char *Workload_GetName(WorkloadLevel_t level);


#endif /* WORKLOAD_H */
