/*
 * system_resource.h
 *
 *  Created on: Oct 23, 2026
 *      Author: swaro
 */

#ifndef SYSTEM_RESOURCE_H
#define SYSTEM_RESOURCE_H

#include "main.h"  /* for HAL_RCC_GetHCLKFreq() */
#include <stdint.h>

/* ============================================================
 * System Resource Monitoring — CPU frequency, stack usage, heap usage
 * ============================================================
 * These three signals correspond directly to the "CPU frequency, workload
 * and memory usage" inputs named in the project's problem statement.
 * Workload itself is covered separately by workload_generator.h.
 *
 * IMPORTANT CURRENT LIMITATIONS (expected, not bugs):
 * - CPU frequency will read a CONSTANT value (matches SYSCLK/HCLK, currently
 *   180MHz) until Stage 3 (DVFS-like clock scaling) is implemented. Logging
 *   it now means the field/pipeline is ready for when it starts varying.
 * - Heap usage will likely read ~0 bytes until something in the firmware
 *   actually calls malloc() (e.g. a future X-CUBE-AI generated model that
 *   allocates buffers dynamically). Not a bug — there's just no heap
 *   allocation happening yet.
 * - Stack usage DOES vary meaningfully today, since it reflects actual call
 *   depth/local variable usage across whatever code has run so far.
 * ============================================================ */

/* Call ONCE, as early as possible in main() — ideally right after HAL_Init(),
 * before any deep function calls happen. This paints the unused portion of
 * the stack with a known pattern, which System_GetStackUsedBytes() later
 * scans to determine the high-water mark. Painting late (after deep calls
 * have already happened) means those early stack depths won't be captured. */
void System_ResourceInit(void);

/* Returns the current CPU core clock in Hz (HCLK, which equals AHB/CPU
 * frequency since AHB prescaler is /1 in this project's clock config). */
uint32_t System_GetCpuFreqHz(void);

/* Returns the stack high-water mark in bytes — the deepest the stack has
 * ever gone since System_ResourceInit() was called, not just the current
 * depth at the moment of the call. This is the standard, more meaningful
 * embedded metric (same technique FreeRTOS uses for stack watermarking). */
uint32_t System_GetStackUsedBytes(void);

/* Returns bytes allocated on the heap since startup (current heap break
 * minus heap start). Will read ~0 until something calls malloc(). */
uint32_t System_GetHeapUsedBytes(void);

#endif /* SYSTEM_RESOURCE_H */
