/*
 * telemetry.h
 *
 *  Created on: Sep 4, 2026
 *      Author: swaro
 */

#ifndef TELEMETRY_H
#define TELEMETRY_H

#include "main.h"

#include <stdint.h>


/*
 * Initialize telemetry subsystem.
 */
void Telemetry_Init(void);


/*
 * Send CSV header.
 */
void Telemetry_SendHeader(void);


/*
 * Send one telemetry sample.
 */
void Telemetry_SendSample(
    uint32_t timestamp_ms,
    uint32_t cpu_frequency_hz,
    float cpu_load_percent,
    uint32_t static_ram_bytes,
    float static_ram_percent,
    const char *workload_name
);


#endif /* TELEMETRY_H */
