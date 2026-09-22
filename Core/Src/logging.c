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
    (void)level; /* reserved for future runtime-adjustable filtering; compile-time filtering
                    already happens in the LOG_* macros before this function is even called */

    if (log_huart == NULL) {
        return; /* Log_Init() not called yet — drop silently rather than fault */
    }

    char buf[160];

    /* Header: [timestamp_ms][LEVEL][MODULE]  */
    int len = snprintf(buf, sizeof(buf), "[%05lu][%s][%s] ",
                        (unsigned long)HAL_GetTick(), level_str, tag);

    if (len < 0 || len >= (int)sizeof(buf)) {
        len = 0; /* header alone overflowed the buffer somehow; bail safely, message still attempts to print */
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
        total_len = (int)sizeof(buf) - 3; /* truncate, leave room for \r\n */
    }

    buf[total_len++] = '\r';
    buf[total_len++] = '\n';

    HAL_UART_Transmit(log_huart, (uint8_t *)buf, (uint16_t)total_len, 100);
}

void Log_Data(uint32_t timestamp_ms, float power_mw, float voltage_v, float current_ma,
              float temp_primary_c, float temp_ambient_c, uint8_t workload_level)
{
#if DATA_LOGGING_ENABLED
    if (log_huart == NULL) {
        return;
    }

    char buf[128];
    int len = snprintf(buf, sizeof(buf),
                        "DATA,%lu,%.2f,%.3f,%.2f,%.2f,%.2f,%u\r\n",
                        (unsigned long)timestamp_ms,
                        (double)power_mw, (double)voltage_v, (double)current_ma,
                        (double)temp_primary_c, (double)temp_ambient_c,
                        (unsigned)workload_level);

    if (len < 0) {
        return;
    }
    if (len >= (int)sizeof(buf)) {
        len = (int)sizeof(buf) - 1; /* truncate rather than overflow */
    }

    HAL_UART_Transmit(log_huart, (uint8_t *)buf, (uint16_t)len, 100);
#else
    (void)timestamp_ms; (void)power_mw; (void)voltage_v; (void)current_ma;
    (void)temp_primary_c; (void)temp_ambient_c; (void)workload_level;
#endif
}


