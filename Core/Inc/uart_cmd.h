/*
 * uart_cmd.h
 *
 *  Line-based commands typed in the serial terminal (115200 8N1, end with Enter):
 *    HELP                     list commands
 *    BAT <mAh> <volts>        set the what-if battery, e.g. BAT 2000 3.7
 *    BAT                      show battery settings
 *    MODE RUN|COLLECT|AUTO    RUN = predict + react, COLLECT = log only,
 *                             AUTO = log only with a random workload schedule
 *    WL <0..3>                set workload IDLE/LOW/MEDIUM/HIGH
 *    WL POT                   workload follows the pot dial on PA0 (A0)
 *    FAN <0..100> | FAN AUTO  manual fan test / back to automatic
 *    LIM <caution> <critical> rise limits in degC above baseline, e.g. LIM 5 8
 *    BASE                     re-measure the idle baseline temperature
 *    LED <r> <g> <b> | LED AUTO  LED wiring test, 0..100 per colour
 */

#ifndef UART_CMD_H
#define UART_CMD_H

#include "usart.h"
#include <stdint.h>

void Cmd_Init(UART_HandleTypeDef *huart);

/* Non-zero when a full line is waiting (set from the UART interrupt) */
extern volatile uint8_t cmd_line_ready;

/* Executes a waiting command, if any. Call from the main loop. */
void Cmd_Poll(void);

#endif /* UART_CMD_H */
