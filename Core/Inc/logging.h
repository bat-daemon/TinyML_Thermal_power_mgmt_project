/*
 * logging.h
 *
 *  Created on: Sep 14, 2026
 *      Author: swaro
 */

#ifndef LOGGING_H
#define LOGGING_H

#include "usart.h"   /* CubeMX-generated — provides huart2 handle + UART_HandleTypeDef */
#include <stdint.h>

/* ============================================================
 * Log Levels
 * ============================================================ */
#define LOG_LEVEL_NONE   0
#define LOG_LEVEL_ERROR  1
#define LOG_LEVEL_WARN   2
#define LOG_LEVEL_INFO   3
#define LOG_LEVEL_DEBUG  4

/* Build-time verbosity control. Lower value = less UART traffic. */
#define CURRENT_LOG_LEVEL   LOG_LEVEL_INFO

/* Independent of CURRENT_LOG_LEVEL — gates the DATA, CSV channel only. */
#define DATA_LOGGING_ENABLED 1

/* ============================================================
 * Module Tags (see ARCHITECTURE.md pipeline stages)
 * ============================================================ */
#define TAG_SENS   "SENS"   /* Sensor Acquisition */
#define TAG_WKLD   "WKLD"   /* Workload Generator */
#define TAG_FEAT   "FEAT"   /* Feature Extraction */
#define TAG_ML     "ML"     /* ML Inference */
#define TAG_REACT  "REACT"  /* Reaction/Battery Calc */
#define TAG_DISP   "DISP"   /* Display/Output */
#define TAG_SYS    "SYS"    /* System Resource Monitoring */

/* ============================================================
 * Public API
 * ============================================================ */

void Log_Init(UART_HandleTypeDef *huart);

void Log_Write(uint8_t level, const char *level_str, const char *tag, const char *fmt, ...);

/* Emits one DATA,-prefixed CSV row. UPDATED field order — now includes
 * CPU frequency and stack/heap usage per the project's problem statement
 * ("CPU frequency, workload and memory usage"). Field order must match the
 * PC-side pyserial script:
 * DATA,timestamp_ms,power_mw,voltage_v,current_ma,temp_primary_c,
 *      temp_ambient_c,workload_level,cpu_freq_hz,stack_used_bytes,heap_used_bytes */
void Log_Data(uint32_t timestamp_ms, float power_mw, float voltage_v, float current_ma,
              float temp_primary_c, float temp_ambient_c, uint8_t workload_level,
              uint32_t cpu_freq_hz, uint32_t stack_used_bytes, uint32_t heap_used_bytes);

/* ============================================================
 * Logging Macros — compile-time filtered.
 * ============================================================ */

#if CURRENT_LOG_LEVEL >= LOG_LEVEL_ERROR
  #define LOG_ERROR(tag, fmt, ...) Log_Write(LOG_LEVEL_ERROR, "ERROR", tag, fmt, ##__VA_ARGS__)
#else
  #define LOG_ERROR(tag, fmt, ...) ((void)0)
#endif

#if CURRENT_LOG_LEVEL >= LOG_LEVEL_WARN
  #define LOG_WARN(tag, fmt, ...) Log_Write(LOG_LEVEL_WARN, "WARN", tag, fmt, ##__VA_ARGS__)
#else
  #define LOG_WARN(tag, fmt, ...) ((void)0)
#endif

#if CURRENT_LOG_LEVEL >= LOG_LEVEL_INFO
  #define LOG_INFO(tag, fmt, ...) Log_Write(LOG_LEVEL_INFO, "INFO", tag, fmt, ##__VA_ARGS__)
#else
  #define LOG_INFO(tag, fmt, ...) ((void)0)
#endif

#if CURRENT_LOG_LEVEL >= LOG_LEVEL_DEBUG
  #define LOG_DEBUG(tag, fmt, ...) Log_Write(LOG_LEVEL_DEBUG, "DEBUG", tag, fmt, ##__VA_ARGS__)
#else
  #define LOG_DEBUG(tag, fmt, ...) ((void)0)
#endif

#endif /* LOGGING_H */
