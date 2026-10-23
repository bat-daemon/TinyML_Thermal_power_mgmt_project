/*
 * system_resource.c
 *
 *  Created on: Oct 23, 2026
 *      Author: swaro
 */

#include "system_resource.h"

/* ============================================================
 * Linker script symbols (standard names in STM32CubeIDE-generated .ld files)
 * ============================================================
 * _estack        — address of the top of the main stack (stack grows down
 *                  from here). Set as the initial SP value in the startup file.
 * _Min_Stack_Size — the reserved stack size in bytes, as configured in the
 *                  linker script (STM32CubeIDE default is usually 0x400 = 1KB).
 *
 * If your linker script uses different symbol names, check
 * STM32F446RETX_FLASH.ld in your project for the actual names and update
 * the extern declarations below to match.
 * ============================================================ */
extern uint32_t _estack;
extern uint32_t _Min_Stack_Size;

/* Standard newlib/GNU ARM linker symbol marking the start of the heap
 * (end of .bss). Used to compute heap usage via the current break pointer. */
extern char end;
extern void *_sbrk(int incr);

#define STACK_CANARY_PATTERN 0xA5A5A5A5u

/* Reads the current Main Stack Pointer. CMSIS intrinsic, available on all
 * Cortex-M cores via core_cm4.h (pulled in transitively through main.h/HAL). */
static inline uint32_t get_current_sp(void)
{
    return __get_MSP();
}

void System_ResourceInit(void)
{
    uint32_t *stack_top    = (uint32_t *)&_estack;
    uint32_t  stack_words  = ((uint32_t)&_Min_Stack_Size) / 4u;
    uint32_t *stack_bottom = stack_top - stack_words;
    uint32_t *current_sp   = (uint32_t *)get_current_sp();

    /* Paint everything from the bottom of the reserved stack region up to
     * (but not including) the current SP — that region above current_sp is
     * live stack content right now and must not be touched. */
    for (uint32_t *p = stack_bottom; p < current_sp; p++) {
        *p = STACK_CANARY_PATTERN;
    }
}

uint32_t System_GetCpuFreqHz(void)
{
    return HAL_RCC_GetHCLKFreq();
}

uint32_t System_GetStackUsedBytes(void)
{
    uint32_t *stack_top    = (uint32_t *)&_estack;
    uint32_t  stack_words  = ((uint32_t)&_Min_Stack_Size) / 4u;
    uint32_t *stack_bottom = stack_top - stack_words;

    uint32_t *p = stack_bottom;
    while (p < stack_top && *p == STACK_CANARY_PATTERN) {
        p++;
    }
    /* p now points at the first word (from the bottom) that's no longer the
     * canary pattern — i.e. the deepest point the stack has ever reached. */
    return (uint32_t)((stack_top - p) * 4u);
}

uint32_t System_GetHeapUsedBytes(void)
{
    char *heap_break = (char *)_sbrk(0);   /* peek current break without allocating */
    return (uint32_t)(heap_break - &end);
}

