/*
 * logging.c
 *
 *  Created on: Sep 14, 2026
 *      Author: swaro
 */
#include "logging.h"
#include <stdio.h>
#include <stdarg.h>

static UART_HandleTypeDef *log_huart = NULL;

void Log_Init(UART_HandleTypeDef *huart)
{
    log_huart = huart;
}

void Log_Write(uint8_t level, const char *level_str, const char *tag, const char *fmt, ...)
{
    (void)level;

    if (log_huart == NULL) {
        return;
    }

    char buf[160];

    int len = snprintf(buf, sizeof(buf), "[%05lu][%s][%s] ",
                        (unsigned long)HAL_GetTick(), level_str, tag);

    if (len < 0 || len >= (int)sizeof(buf)) {
        len = 0;
    }

    va_list args;
    va_start(args, fmt);
    int msg_len = vsnprintf(buf + len, sizeof(buf) - (size_t)len, fmt, args);
    va_end(args);

    if (msg_len < 0) {
        msg_len = 0;
    }

    int total_len = len + msg_len;
    if (total_len > (int)sizeof(buf) - 3) {
        total_len = (int)sizeof(buf) - 3;
    }

    buf[total_len++] = '\r';
    buf[total_len++] = '\n';

    HAL_UART_Transmit(log_huart, (uint8_t *)buf, (uint16_t)total_len, 100);
}

void Log_Data(uint32_t timestamp_ms, float power_mw, float voltage_v, float current_ma,
              float temp_primary_c, float temp_ambient_c, uint8_t workload_level,
              uint32_t cpu_freq_hz, uint32_t stack_used_bytes, uint32_t heap_used_bytes)
{
#if DATA_LOGGING_ENABLED
    if (log_huart == NULL) {
        return;
    }

    char buf[160];   /* widened from 128 to fit the three new fields */
    int len = snprintf(buf, sizeof(buf),
                        "DATA,%lu,%.2f,%.3f,%.2f,%.2f,%.2f,%u,%lu,%lu,%lu\r\n",
                        (unsigned long)timestamp_ms,
                        (double)power_mw, (double)voltage_v, (double)current_ma,
                        (double)temp_primary_c, (double)temp_ambient_c,
                        (unsigned)workload_level,
                        (unsigned long)cpu_freq_hz,
                        (unsigned long)stack_used_bytes,
                        (unsigned long)heap_used_bytes);

    if (len < 0) {
        return;
    }
    if (len >= (int)sizeof(buf)) {
        len = (int)sizeof(buf) - 1;
    }

    HAL_UART_Transmit(log_huart, (uint8_t *)buf, (uint16_t)len, 100);
#else
    (void)timestamp_ms; (void)power_mw; (void)voltage_v; (void)current_ma;
    (void)temp_primary_c; (void)temp_ambient_c; (void)workload_level;
    (void)cpu_freq_hz; (void)stack_used_bytes; (void)heap_used_bytes;
#endif
}
