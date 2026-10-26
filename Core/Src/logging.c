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

    char buf[256];   /* status lines run past 160 chars */

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

void Log_Data(const DataRecord_t *rec)
{
#if DATA_LOGGING_ENABLED
    if (log_huart == NULL || rec == NULL) {
        return;
    }
    static char buf[224];   /* static: keeps ~200 bytes off the 4 KB stack */
    int len = snprintf(buf, sizeof(buf),
                        "DATA,%lu,%.2f,%.3f,%.2f,%.2f,%.2f,%.2f,%u,%u,%lu,%lu,%lu,%u,%u,%u,%u,%u\r\n",
                        (unsigned long)rec->timestamp_ms,
                        (double)rec->power_mw, (double)rec->voltage_v, (double)rec->current_ma,
                        (double)rec->temp_die_c, (double)rec->temp_rise_c, (double)rec->temp_ambient_c,
                        (unsigned)rec->workload_req, (unsigned)rec->workload_eff,
                        (unsigned long)rec->cpu_freq_hz,
                        (unsigned long)rec->stack_used_bytes,
                        (unsigned long)rec->heap_used_bytes,
                        (unsigned)rec->fan_pct,
                        (unsigned)rec->temp_risk, (unsigned)rec->power_risk,
                        (unsigned)rec->rule_temp_risk, (unsigned)rec->mode);
    if (len < 0) {
        return;
    }
    if (len >= (int)sizeof(buf)) {
        len = (int)sizeof(buf) - 1;
    }
    HAL_UART_Transmit(log_huart, (uint8_t *)buf, (uint16_t)len, 100);
#else
    (void)rec;
#endif
}
